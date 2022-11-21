#pragma once

#include <vector>
#include <list>

#include <anncore/transport.hpp>


// custom payload, must be a POD
struct payload_t {
	// id and reference count
	payload_id_t id     = -1;
	ptrdiff_t ref_count = 0;

	// value of the payload
	void *value        = nullptr;
};


// poor man's memory management with a slab memory system to reduce memory
// allocations.
struct payload_slab {
	// slab to store payloads for message transport
	std::vector<payload_t>	slab(0);

	std::list<size_t>		free_slots;
	size_t					last_idx = 0;
	const size_t			prealloc_size = 64;

	// some additional statistics
	size_t					total_allocated = 0;
	size_t					total_released  = 0;
};


/*
 * payload_get - get pointer to a certain payload.
 *
 * This returns a const pointer to the payload. There might be other consumers
 * of the payload, so avoid changing it. Only the owner (i.e. creator) of the
 * resource should change it. If you're absolutely sure that you received a copy
 * of original data, and you're the only consumer, of course you can cast away
 * the constness.
 */
inline
const payload_t*
payload_get(payload_slab &slab, payload_id_t pid)
{
	return &slab.slab[pid];
}


/*
 * payload_reset - reset the content/values of a payload
 */
inline
void
payload_reset_values(payload_t *payload)
{
	payload->value = nullptr;
}


/*
 * payload_copy_values - copy the 'content' of a payload
 *
 * This function is intended to copy the values of a payload from origin to
 * target. Any memory reference information that is stored within a payload is
 * not modified.
 */
inline
void
payload_copy_values(const payload_t *origin, payload_t *target)
{
	target->value = origin->value;
}


/*
 * payload_new - Get a pointer to a new payload
 *
 * Returns a pointer to a payload_t. The memory used for the payload can be
 * either newly allocated, or re-used memory of a prior payload whose reference
 * count dropped to 0.
 */
inline
payload_t*
payload_new(payload_slab &slab)
{
	payload_t *result = nullptr;

	// re-use memory that was freed in a previous run, or use last slab space element?
	if (slab.free_slots.size() > 0) {
		size_t id = slab.free_slots.front();
		slab.free_slots.pop_front();

		result = &slab.slab[id];
		result->id = id;
	}
	else {
		// need to allocate more memory?
		if (slab.last_idx >= slab.slab.size())
			slab.slab.resize(slab.slab.size() + slab.prealloc_size);

		result = &slab.slab[slab.last_idx];
		result->id = slab.last_idx++;
	}

	// set ref-count to 1, and reset values stored in the payload
	result->ref_count = 1;
	payload_reset_values(result);

	// additional statistics
	++slab.total_allocated;

	return result;
}


/*
 * __payload_release - release memory of a payload
 *
 * This returns the memory used by a payload. In the implementation here, this
 * means that the memory location is appended to the list of free memory slots
 * in the payload slab.
 *
 * Note: this function is for internal use only, do not explicitly call it from
 * user code.
 */
inline
void
__payload_release(payload_slab &slab, payload_id_t id)
{
	std::cout << "payload_release (id " << id << ")" << std::endl;
	// push the ID into the list of free items
	slab.free_slots.push_back(id);
	// additional statistics
	++slab.total_released;
}


/*
 * payload_free - free a payload
 *
 * This 'frees' a payload. Note that in the implementation here, this simply
 * decreases the reference count. If the reference count drops to zero,
 * __payload_release is invoked.
 *
 * Note: It is in the responsibility of the user to call this function when the
 * memory was 'allocated' using payload_new. Any intermediate call to this
 * function can lead to erroneous behavior. Ideally, only the owner of the
 * payload calls payload_new and payload_free, and any intermediate consumer
 * calls payload_inc_refcount and payload_dec_refcount to indicate additional
 * consumers.
 *
 * Note also that this function must be called in case payload_copy was used.
 */
inline
void
payload_free(payload_slab &slab, payload_t *payload)
{
	if (!payload) return;

	size_t ref_count = payload->ref_count;
	if (ref_count == 1) {
		payload->ref_count = 0;
		__payload_release(slab, payload->id);
	}
	else
		std::cerr << "WW: payload_free called on payload with ref_count = " << ref_count << "\n";
}


/*
 * payload_inc_refcount - increment the reference count of a payload.
 *
 * This function increases the reference count of a payload by 1. To avoid
 * memory leaks, calls to this function should be accompanied by corresponding
 * calls to payload_dec_refcount.
 */
inline
ptrdiff_t
payload_inc_refcount(payload_slab &slab, const payload_id_t id)
{
	return ++(slab.slab[id].ref_count);
}


/*
 * payload_dec_refcount - decrement the reference count of a payload.
 *
 * This function decreases the reference count of a payload by 1. If the
 * reference count drops to 0, the function will release memory assigned to
 * payload by calling __payload_release.
 */
inline
ptrdiff_t
payload_dec_refcount(payload_slab &slab, const payload_id_t id)
{
	auto pl = &slab.slab[id];
	size_t ref_count = pl->ref_count;
	if (ref_count > 0) {
		ref_count = --(pl->ref_count);
		if (ref_count == 0)
			__payload_release(id);
	}
	return ref_count;
}


/*
 * payload_copy - copy a payload
 *
 * This will make an exact copy of a payload's values, and return the id of a
 * new payload. Note that this internally calls payload_new, so the caller of
 * payload_copy is considered the owner of the new resource and must either call
 * payload_free accordingly, or make sure that the reference count drops to 0
 * due to appropriate calls of payload_dec_refcount.
 */
inline
payload_id_t
payload_copy(payload_slab &slab, const payload_id_t id)
{
	auto* origin = payload_get(slab, id);
	auto* pl = payload_new(slab);
	payload_copy_values(origin, pl);
	return pl->id;
}

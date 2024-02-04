/*
 * ncr_transport2 - An improved message-passing & transport subsystem
 *
 * SPDX-FileCopyrightText: 2022-2023 Nicolai Waniek <n@rochus.net>
 * SPDX-License-Identifier: MIT
 * See LICENSE file for more details
 *
 */

#pragma once

#include <cassert>
#include <list>
#include <deque>
#include <vector>
#include <optional>
#include <unordered_set>
#include <unordered_map>
#include <concepts>
#include <functional>

#include <ncr/ncr_units.hpp>
#include <ncr/ncr_common.hpp>
#include <ncr/ncr_memory.hpp>


namespace ncr {

// forward declarations
template <typename Traits>
	requires std::copyable<typename Traits::payload_type>
	      && std::copyable<typename Traits::options_type>
struct envelope;

template <typename Traits>
	requires std::copyable<typename Traits::payload_type>
	      && std::copyable<typename Traits::options_type>
struct port;

template <typename Traits>
	requires std::copyable<typename Traits::payload_type>
	      && std::copyable<typename Traits::options_type>
struct transport;


// as index type, simply use the slab memory index type
using envelope_index = slab_memory_index_t;


/*
 * struct null_options - a struct that represents an empty set of options
 *
 * Additional transport options can be used to influence when or how messages
 * will be delivered. For instance, one could define options that encode a
 * delay. For this to work, the following templates must be further specialized.
 *
 * Note: Internally, the transport does *not* use any options passed to it. They
 *       will be part of the message that will be sent around. More precisely,
 *       each payload that the transport shall deliver is wrapped into an
 *       'envelope'. The options are part of this envelope, and can be used by a
 *       user of the transport system to, for instance to deliver messages at
 *       a certain time or after a number of ticks of simulations.
 *
 * TODO: rename (empty_options? put into a namespace? ncr::TransportOptions::empty?)
 */
struct null_options { };


/*
 * struct envelope_t - a generic envelope around a payload
 *
 * A envelope_t is wrapped around a payload that gets to be submitted. The
 * envelope will contain additional information in its header, such as how long
 * the transport shall be delayed, and in which Timing Mode it was submitted.
 */
template <typename Traits>
	requires std::copyable<typename Traits::payload_type>
	      && std::copyable<typename Traits::options_type>
struct envelope {

	// id/signature for this message
	struct {
	    size_t source; // source ID
		size_t sink; // sink ID
	    size_t msg; // message ID relative to source
	} id;

	// additional options that will be required for sorting and delivery
	// evaluation. Note that
	typename Traits::options_type options;

	// payload of this message at the end of the struct
	typename Traits::payload_type payload;
};


/*
 * port_index_t - Type to identify ports numerically
 */
using port_index = std::size_t;


/*
 * struct port - Connection point to handle payload of type T
 *
 * To communicate messages with other entities, a class must have at least one
 * port. On arrival of a new message, the transport subsystem will put the
 * message into the port's buffer.
 */
template <typename Traits>
	requires std::copyable<typename Traits::payload_type>
	      && std::copyable<typename Traits::options_type>
struct port {

	using envelope_type = envelope<Traits>;

	// pointer to the transport this port belongs to
	struct transport<Traits> *transport;

	// every port has an ID. This is set by the *mandatory* call to register the
	// port with a transport
	std::optional<port_index> index = {};

	// every port can produce messages. Each message is numbered, and to do so,
	// this value is used.
	// TODO: change type, and set to -1 or so
	size_t last_msg_id = 0;

	// mail-buffer for incoming messages
	// TODO: maybe change instance to unique_ptr ?
	std::deque<envelope_index> buffer;
};


/*
 * port_clear_buffer - clear the buffer of a port
 *
 * This will release memory on every envelope within the buffer, and afterwards
 * clear the buffer itself.
 */
template <typename T>
void
port_clear_buffer(port<T> &p)
{
	for (size_t i = 0; i < p.buffer.size(); ++i)
		free_envelope(p.transport, p.buffer[i]);
	p.buffer.clear();
}


/*
 * port_clear_buffers - clear the buffer of several ports, base case
 */
template <typename T>
void
port_clear_buffers(port<T> &p)
{
	port_clear_buffer(p);
}


/*
 * port_clear_buffers - clear the buffer of several ports
 */
template <typename T, typename... Ts>
void
port_clear_buffers(
		port<T> &p,
		Ts&&... ps) requires (std::same_as<decltype(p), Ts> && ...)
{
	port_clear_buffers(p);
	port_clear_buffers(std::forward<Ts>(ps)...);
}


/*
 * port_drain - remove all pending messages on a port
 */
template <typename T>
void
port_drain(port<T> *p)
{
	port_clear_buffer(*p);
}


/*
 * port_drain - remove all pending messages on a port
 */
template <typename T>
void
port_drain(port<T> &p)
{
	port_clear_buffer(p);
}


/*
 * struct __transport_map_t - internally used map
 *
 * This map stores all connections between two ports. Specifically, it stores
 * both the forward-edges from sources to sinks, as well as all reverse edges
 * from sinks to sources.
 * Note that we don't store pointers, but IDs to ports. This is a level of
 * indirection which helps to prevent accessing ports which are not known
 * anymore.
 */
template <typename Traits>
	requires std::copyable<typename Traits::payload_type>
	      && std::copyable<typename Traits::options_type>
struct __transport_map
{
	// forward transport map, which links a source to all sinks
	std::unordered_map<size_t, std::unordered_set<size_t>> forward;

	// reverse transport map, which links all sinks to their sources
	std::unordered_map<size_t, std::unordered_set<size_t>> reverse;
};




/*
 * struct transport_t - transport information to send around messages
 *
 * This struct implements an internal buffer mechanism to transport messages
 * from sources to sinks, accounting also for potential temporal delays between
 * submission and delivery.
 */
template <typename Traits>
	requires std::copyable<typename Traits::payload_type>
	      && std::copyable<typename Traits::options_type>
struct transport
{
	typedef __transport_map<Traits>          map_type;
	typedef envelope<Traits>                 envelope_type;
	typedef port<Traits>                     port_type;
	typedef std::list<envelope_index>        buffer_type;

	// list of all known ports
	std::unordered_map<size_t, port<Traits>*>  known_ports;

	// transport map, i.e. where to submit message to
	map_type map;

	// buffer to store message
	buffer_type buffer;

	// internal variable which gets incremented everytime a port gets
	// registered. Registering a port at the moment simply means giving it a
	// unique ID, determined by this variable here.
	size_t last_port_id = 0;

	// memory manager for envelopes
	slab_memory<envelope_type> __mem_envelopes;

	typedef std::function<bool(const envelope_type&, const envelope_type&)> CompareEnvelopes;

	/*
	 * struct back_inserter - add messags to the back of the buffer
	 *
	 * A back-inserter can be used, for instance, when creating a time-less
	 * transport for which additional messages should simply be put at the end
	 * of the internal message buffer.
	 *
	 * Usage Example:
	 *
	 *		// a simple type trait which tells the transport the payload type as
	 *		// well as the options_type (here: null_tyupe
	 *		struct TTypeTraits {
	 *			using payload_type = some_payload;
	 *			using options_type = null_options;
	 *		};
	 *
	 *		// declare a (timeless) transport which simply puts all messages at
	 *		// the end of the internal buffer
	 *      using transport_type = transport<TTypeTraits>;
	 *      transport_type transport(transport_type::back_inserter{});
	 */
	struct back_inserter {
		bool operator()(const envelope_type &, const envelope_type &) { return true; }
	};

	/*
	 * struct front_inserter - add messages to the front of the buffer.
	 *
	 * A front-inserter can be used, for instance, when creating a time-less
	 * transport for which additional messages should be put in front of other,
	 * already existing messages within the internal message buffer.
	 *
	 * For an example, see struct back_inserter.
	 */
	struct front_inserter {
		bool operator()(const envelope_type &, const envelope_type &) { return false; }
	};

	// TODO: maybe turn into callable struct, so that it can be accessed via
	//       TransportType::accept_all ?
	typedef std::function<bool(const typename transport<Traits>::envelope_type)> DeliveryAttempt;
	const DeliveryAttempt accept_all = [](const envelope_type &){ return true; };
	const DeliveryAttempt reject_all = [](const envelope_type &){ return false; };

	// comparison function for envelopes to make sure they are inserted
	// internally in order of their delivery. This is something that must be
	// specified externally and passed in the constructor.
	CompareEnvelopes comp_envs;

	transport(CompareEnvelopes comp) : comp_envs(comp) { assert(comp); }
};


/*
 * alloc_envelope - allocate memory for an envelope
 */
template <typename T>
envelope_index
alloc_envelope(transport<T> &transport)
{
	// TODO: check if value exists
	return transport.__mem_envelopes.alloc().value();
}

/*
 * alloc_envelope - allocate memory for an envelope
 */
template <typename T>
envelope_index
alloc_envelope(transport<T> *transport)
{
	return alloc_envelope(*transport);
}

/*
 * free_envelope - release memory acquired for an envelope
 */
template <typename T>
void free_envelope(
		transport<T> &transport,
		const envelope_index env_index)
{
	transport.__mem_envelopes.free(env_index);
}


/*
 * free_envelope - release memory acquired for an envelope
 */
template <typename T>
void free_envelope(
		transport<T> *transport,
		const envelope_index env_index)
{
	free_envelope(*transport, env_index);
}


/*
 * transport_get_envelope - get a pointer to an envelope
 *
 * TODO: check value returned by __mem_envelopes.get(), and maybe also return an
 * optional here
 */
template <typename T>
typename transport<T>::envelope_type*
transport_get_envelope(
		transport<T> *transport,
		const envelope_index env_index)
{
	return transport->__mem_envelopes.get(env_index).value();
}


/*
 * transport_get_envelope - get a pointer to an envelope
 *
 * This is an overloaded version of transport_get_envelope which accepts a
 * pointer to transport.
 */
template <typename T>
typename transport<T>::envelope_type*
transport_get_envelope(
		transport<T> &transport,
		const envelope_index env_index)
{
	return transport_get_envelope(&transport, env_index);
}


/*
 * connect - connect a source and sink using a given transport via port IDs
 */
template <typename T>
void connect(
		transport<T> &transport,
		std::optional<port_index> source_id,
		std::optional<port_index> sink_id)
{
	if (!source_id || !sink_id) return;

	// store in the forward map
	transport.map.forward[source_id.value()].insert(sink_id.value());
	// also store the source in the reverse map of the sink
	transport.map.reverse[sink_id.value()].insert(source_id.value());
}


/*
 * connect - connect a source and sink using a given transport via pointers
 */
template <typename T>
void connect(
		transport<T> &transport,
		port<T> *source,
		port<T> *sink)
{
	assert(source);
	assert(sink);
	connect(transport, source->index, sink->index);
}


/*
 * connect - connect two or more sinks to a given transport using port IDs.
 */
template <typename T, typename... Ts>
void connect(
		transport<T> &transport,
		std::optional<port_index> source_id,
		std::optional<port_index> sink_id,
		Ts... sink_ids) requires (std::same_as<decltype(sink_id), Ts> && ...)
{
	connect(transport, source_id, sink_id);
	connect(transport, source_id, sink_ids...);
}


/*
 * connect - connect two or more sinks to a given transport using pointers
 */
template <typename T, typename... Ts>
void connect(
		transport<T> &transport,
		port<T> *source,
		port<T> *sink,
		Ts... sinks) requires (std::same_as<decltype(sink), Ts> && ...)
{
	connect(transport, source->index, sink->index);
	connect(transport, source, sinks...);
}





/*
 * disconnect - disconnect a sink from a source using port IDs
 */
template <typename T>
void disconnect(
		transport<T> &transport,
		std::optional<port_index> source_id,
		std::optional<port_index> sink_id)
{
	if (!source_id || !sink_id) return;

	// remove forward link from forward map
	auto needle = transport.map.forward.find(source_id.value());
	if (needle != transport.map.forward.end()) {
		auto &map = needle->second;
		map.erase(sink_id.value());
	}

	// remove reverse link from reverse map
	// transport.map.reverse.erase(sink_id);
	needle = transport.map.reverse.find(sink_id.value());
	if (needle != transport.map.reverse.end()) {
		auto &map = needle->second;
		map.erase(source_id.value());
	}
}

/*
 * disconnect - disconnect a port, essentially removing it from the transport
 *
 * Note that the port will still be registered.
 */
template <typename T>
void disconnect(
	transport<T> &transport,
	std::optional<port_index> sink_id)
{
	if (!sink_id) return;

	// find all sources pointing to this sink
	auto needle = transport.map.reverse.find(sink_id.value());
	if (needle == transport.map.reverse.end())
		return;

	auto sources = needle->second;
	for (const auto &source_id: sources)
		disconnect(transport, source_id, sink_id);
}


/*
 * disconnect - disconnect a sink from a source using pointers
 */
template <typename T>
void disconnect(
		transport<T> &transport,
		port<T>* source,
		port<T>* sink)
{
	assert(source);
	assert(sink);
	disconnect(transport, source->index, sink->index);
}

/*
 * disconnect - disconnect two or more sinks from a source using port IDs
 */
template <typename T, typename... Ts>
void disconnect(
		transport<T> &transport,
		std::optional<port_index> source_id,
		std::optional<port_index> sink_id,
		Ts... sink_ids) requires (std::same_as<decltype(sink_id), Ts> && ...)
{
	disconnect(transport, source_id, sink_id);
	disconnect(transport, source_id, sink_ids...);
}

/*
 * disconnect - disconnect two or more sinks from a source using pointers
 */
template <typename T, typename... Ts>
void disconnect(
		transport<T> &transport,
		port<T> *source,
		port<T> *sink,
		Ts... sinks) requires (std::same_as<decltype(sink), Ts> && ...)
{
	disconnect(transport, source->index, sink->index);
	disconnect(transport, source, sinks...);
}


template <typename T>
port<T>*
__get_port(transport<T> &transport, port_index id)
{
	auto needle = transport.known_ports.find(id);
	if (needle != transport.known_ports.end())
		return needle->second;
	return nullptr;
}


// TODO: rename this function
template <typename T>
void
process_messages(transport<T> &tr,
		typename transport<T>::DeliveryAttempt attempt_delivery)
{
	assert(attempt_delivery);

	using envelope_type = typename transport<T>::envelope_type;
	using port_type     = typename transport<T>::port_type;

	// select the proper function to use for time evaluation
	// auto check_delivery_fn = transport.__check_delivery_fns[transport.time_mode];

	// mailbuffer is sorted by timestamp, so we can actually break as soon as we
	// reached a message which doesn't need delivery
	for (auto it = tr.buffer.begin(); it != tr.buffer.end();) {

		// the envelope ID is simply what is stored in the buffer
		envelope_index env_id = *it;

		// TODO: move into a function, also check if the value actually exists
		const envelope_type *env = tr.__mem_envelopes.get(env_id).value();

		// attempt to deliver the envelope. This depends on external factors,
		// for instance time of the simulation and options present in an
		// envelope, which is why it is mapped to a function that is passed in.
		if (attempt_delivery(*env)) {
			// try to find the port. The port, and thereby its pointer, might
			// have gone away or become invalid in the meantime, so we try to
			// locate it in the list of known ports
			port_type *sink = __get_port(tr, env->id.sink);
			if (sink != nullptr)
				// in this case, the recipient must delete all envelopes
				sink->buffer.push_back(env_id);
			else
				// delete memory allocated for the message
				free_envelope(tr, env_id);

			// remove from list and go to the next item
			it = tr.buffer.erase(it);
		}
		else {
			// delivering this envelope was not possible, so we can brak right
			// here - reason: we assume that internally all envelopes are
			// ordered in their delivery order, e.g. relative in time
			break;
		}
	}
}


template <typename T>
void
__mailbuffer_insert(
		transport<T> &transport,
		const envelope_index new_env_id)
{
	// TODO: find the right place to insert stuff, ideally temporally ordered

	// assume that new mails will be mostly appended to the list. still, they
	// should be added at the correct location. Hence, we search from the back
	// until we find the proper slot to insert the element based on the
	// transport's function comp_envs
	auto rbegin = transport.buffer.rbegin();
	auto rend   = transport.buffer.rend();

	auto new_envelope = transport_get_envelope(transport, new_env_id);
	while (rbegin != rend) {
		auto left = transport_get_envelope(transport, *rbegin);
		if (transport.comp_envs(*left, *new_envelope))
			break;
		++rbegin;
	}

	transport.buffer.insert(rbegin.base(), new_env_id);
}


/*
 * send - 'needle-cast' a message from source_id to sink_id
 *
 * Send a certain message from source_id to sink_id. This will work even if the
 * connection between source and sink is not stored within the transport's map.
 */
template <typename T>
void
send(
		transport<T> &transport,
		std::optional<port_index> source_id,
		std::optional<port_index> sink_id,
		typename T::payload_type payload,
		typename T::options_type options)
{
	if (!source_id || !sink_id) return;

	// the easiest implementation simply copies the message to all recipient's
	// bufferes, but this would not account for any timing information that
	// needs to be accounted for, such as delayed lines, and similar things.
	//
	// Hence, we build a slightly more interesting message around the payload

	// first step, try to figure out if the source and sink are known ports
	auto source = __get_port(transport, source_id.value());
	auto sink   = __get_port(transport, sink_id.value());
	if (source == nullptr || sink == nullptr)
		return;

	// allocate and get memory
	// TODO: check optional if there exists a proper value
	envelope_index env_id = alloc_envelope(transport);
	auto *envelope = transport.__mem_envelopes.get(env_id).value();

	// fill ID fields
	envelope->id.source = source_id.value();
	envelope->id.sink   = sink_id.value();
	envelope->id.msg    = source->last_msg_id++;

	// finally move payload resource as well as the options
	// TODO: is the move really the right thing to do here? Probably it's better
	//       to copy the payload, and clearly state in the documentation that
	//       this is a copying transport. Hence, it's recommended to send around
	//       only small objects.
	envelope->options = std::move(options);
	envelope->payload = std::move(payload);

	// in case the transport operates in TTM_NONE, directly put the message into
	// the recipient's buffer. If not, then need to place it into the mail
	// buffer for delivery at the appropriate time.
	// TODO: if delivery is *right now*, then directly put into sink's buffer,
	//       don't store locally
	//if (can_deliver(env)) {
	if (false) {
	//if (transport.time_mode == TTM_NONE) {
		if (sink)
			sink->buffer.push_back(env_id);
		else
			free_envelope(transport, env_id);
	}
	else {
		// insert into mailbuffer
	    __mailbuffer_insert(transport, env_id);
	}
}


/*
 * broadcast - given a source_id, send message to all connected sinks
 */
template <typename T>
inline void
broadcast(
		transport<T> &transport,
		std::optional<port_index> source_id,
		typename T::payload_type payload,
		typename T::options_type opts)
{
	if (!source_id) return;

	auto sinks = transport.map.forward.find(source_id.value());
	if (sinks == transport.map.forward.end())
		return;

	for (auto sink_id : sinks->second)
		send(transport, source_id, sink_id, payload, opts);
}


/*
 * broadcast - given a source_id, send messages to all connected sinks
 */
template <typename T>
inline void
broadcast(
		transport<T> *transport,
		std::optional<port_index> source_id,
		typename T::payload_type payload,
		typename T::options_type opts)
{
	broadcast<T>(*transport, source_id, payload, opts);
}


template <typename T>
inline void
broadcast(
		transport<T> &transport,
		port<T> *source,
		typename T::payload_type payload,
		typename T::options_type opts)
{
	assert(source);
	broadcast<T>(transport, source->index, payload, opts);
}


template <typename T>
inline void
broadcast(
		transport<T> *transport,
		port<T> *source,
		typename T::payload_type payload,
		typename T::options_type opts)
{
	assert(source);
	broadcast<T>(*transport, source->index, payload, opts);
}



/*
 * register_ports - make a port aware to a transport
 *
 * At the moment, this simply sets an ID within the port, and also resets the
 * counter of messages sent by this port.
 */
template <typename T>
inline void
register_ports(
		transport<T> &transport,
		port<T> *p)
{
	assert(p);

	// assign a new ID to this port
	p->transport   = &transport;
	p->index       = transport.last_port_id++;
	p->last_msg_id = 0;

	// store the ID in the transport's list of known ports
	transport.known_ports[p->index.value()] = p;
}


/*
 * unregister_port - remove ports from the given transport
 */
template <typename T>
inline void
unregister_ports(
		transport<T> &transport,
		port<T> *p)
{
	assert(p);
	if (!p->index) return;

	// disconnect this port from everything in the transport system
	disconnect(transport, p->index);

	// remove from known ports
	transport.known_ports.erase(p->index.value());

	// reset the transport pointer, and set index to an unknown state.
	// TODO: could also use optional for the transport
	p->transport = nullptr;
	p->index = {};
}


/*
 * register_ports - make several ports aware to a transport
 */
template <typename T, typename... Ts>
inline void
register_ports(
		transport<T> &transport,
		port<T> *p,
		Ts... ps) requires (std::same_as<decltype(p), Ts> && ...)
{
	register_ports(transport, p);
	register_ports(transport, ps...);
}


/*
 * register_ports - make several ports aware to a transport
 */
template <typename T, typename... Ts>
inline void
register_ports(
		transport<T> *transport,
		port<T> *p,
		Ts... ps) requires (std::same_as<decltype(p), Ts> && ...)
{
	register_ports(*transport, p);
	register_ports(*transport, ps...);
}


template <typename T, typename... Ts>
inline void
unregister_ports(
		transport<T> &transport,
		port<T> *p,
		Ts... ps) requires (std::same_as<decltype(p), Ts> && ...)
{
	unregister_ports(transport, p);
	unregister_ports(transport, ps...);
}


template <typename T, typename... Ts>
inline void
unregister_ports(
		transport<T> *transport,
		port<T> *p,
		Ts... ps) requires (std::same_as<decltype(p), Ts> && ...)
{
	unregister_ports(*transport, p);
	unregister_ports(*transport, ps...);
}


} // ncr::

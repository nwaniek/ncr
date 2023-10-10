/*
 * ncr_transport - A message-passing & transport subsystem
 *
 * SPDX-FileCopyrightText: 2022 Nicolai Waniek <rochus@rochus.net>
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

#include <ncr/ncr_units.hpp>
#include <ncr/ncr_common.hpp>
#include <ncr/ncr_memory.hpp>

// TODO: check if we can avoid importing simulation.hpp
// TODO: merge this with simulator in some manner, such that the user has *one*
//       location to specify how time is treated, i.e. either as ticks, or as
//       floats.
//       The issue with merging is, however, that the transport cannot be used
//       independently of the simulator any longer. So, maybe keep it separated?
//       There is the dependency on the iteration_state to evaluate if a message
//		 needs to be transmitted, though.
#include <ncr/ncr_simulation.hpp>

namespace ncr {


/*
 * The transport can operate in three different modes: NONE, TICK, and TIME. In
 * NONE-mode, the transport will ignore any timing information within messages.
 * In TICK mode, message delays will be computed based on passed ticks as well
 * as the value stored in ticks_delayed. In TIME mode, delivery will be computed
 * based on timestamp and time_delayed.
 *
 * Note: TTM_NONE is a synchronous message passing variant. That is, messages
 * are passed along immediately. With both TTM_TICK and TTM_TIME, asynchronous
 * (distributed) systems can be implemented, because they allow to deliver
 * messages at random times.
 */
enum transport_time_mode {
	TTM_NONE = 0,
	TTM_TICK = 1,
	TTM_TIME = 2,
};
// number of different time modes specified above
constexpr const size_t TRANSPORT_TIME_MODE_COUNT = 3;


/*
 * delivery / transport options for messages
 */
struct transport_options_t {
	union {
		size_t delay_ticks = 0;
		double delay_time;
	};
};


// forward declarations
template <typename T> requires std::copyable<T>
struct port_t;

template <typename T> requires std::copyable<T>
struct transport_t;

// as index type, simply use the slab memory index type
using transport_index_t = slab_memory_index_t;


/*
 * struct envelope_t - a generic envelope around a payload
 *
 * A envelope_t is wrapped around a payload that gets to be submitted. The
 * envelope will contain additional information in its header, such as how long
 * the transport shall be delayed, and in which Timing Mode it was submitted.
 */
template <typename T> requires std::copyable<T>
struct envelope_t {

	// id/signature for this message
	struct {
	    size_t source; // source ID
		size_t sink; // sink ID
	    size_t msg; // message ID relative to source
	} id;

	// header with timing information about this message, and if the message
	// needs to be processed in tick mode or time mode
	struct {
		// timing mode this message operates with
		transport_time_mode time_mode = TTM_NONE;

		// when the message was sent (either tick- or timestamp)
		// TODO: rename
		union {
			size_t ticks = 0;
			double timestamp;
		};

		// how much should this message be delayed
		union {
			size_t ticks_delayed = 0;
			double time_delayed;
		};

		// time the message is due to be delivered.
		union {
			size_t delivery_tick;
			double delivery_time;
		};

	} header;

	// payload of this message at the end of the struct
	T payload;
};


/*
 * port_index_t - Type to identify ports numerically
 */
using port_index_t = std::size_t;


/*
 * struct port_t - Connection point to handle payload of type T
 *
 * To communicate messages with other entities, a class must have at least one
 * port. On arrival of a new message, the transport subsystem will put the
 * message into the port_t's buffer.
 */
template <typename T> requires std::copyable<T>
struct port_t {

	using envelope_type = envelope_t<T>;

	// pointer to the transport this port belongs to
	transport_t<T> *transport;

	// every port has an ID. This is set by the *mandatory* call to register the
	// port with a transport
	std::optional<port_index_t> index = {};

	// every port can produce messages. Each message is numbered, and to do so,
	// this value is used.
	// TODO: change type, and set to -1 or so
	size_t last_msg_id = 0;

	// mail-buffer for incoming messages
	// TODO: maybe change instance to unique_ptr ?
	std::deque<transport_index_t> buffer;
};


/*
 * port_clear_buffer - clear the buffer of a port_t
 *
 * This will release memory on every envelope within the buffer, and afterwards
 * clear the buffer itself.
 */
template <typename T>
void
port_clear_buffer(port_t<T> &port)
{
	for (size_t i = 0; i < port.buffer.size(); ++i)
		free_envelope(port.transport, port.buffer[i]);
	port.buffer.clear();
}


/*
 * port_clear_buffers - clear the buffer of several ports, base case
 */
template <typename T>
void
port_clear_buffers(port_t<T> &port)
{
	port_clear_buffer(port);
}


/*
 * port_clear_buffers - clear the buffer of several ports
 */
template <typename T, typename... Ts>
void
port_clear_buffers(
		port_t<T> &port,
		Ts&&... ports) requires (std::same_as<decltype(port), Ts> && ...)
{
	port_clear_buffers(port);
	port_clear_buffers(std::forward<Ts>(ports)...);
}


/*
 * port_drain - remove all pending messages on a port
 */
template <typename T>
void
port_drain(port_t<T> *port)
{
	port_clear_buffer(*port);
}


/*
 * port_drain - remove all pending messages on a port
 */
template <typename T>
void
port_drain(port_t<T> &port)
{
	port_clear_buffer(port);
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
template <typename T> requires std::copyable<T>
struct __transport_map_t
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
template <typename T> requires std::copyable<T>
struct transport_t
{
	typedef __transport_map_t<T>             map_type;
	typedef envelope_t<T>                    envelope_type;
	typedef std::list<transport_index_t> buffer_type;

	// list of all known ports
	std::unordered_map<size_t, port_t<T>*>  known_ports;

	// transport map, i.e. where to submit message to
	map_type map;

	// buffer to store message
	buffer_type buffer;

	// the transport is usually part of a simulation, and message delivery is
	// relative to simulated time. hence, we'll store a pointer to the
	// iteration_state against which relative times are computed
	//
	// TODO: this could be part of a template argument to get rid of dependency
	//       on simulation.hpp
	const iteration_state * iter_state;

	// in which way are messages handled and processed? either by ticks or by
	// timestamp. By default, the transport ignores any time information in
	// messages
	transport_time_mode time_mode = TTM_NONE;

	// internal variable which gets incremented everytime a port gets
	// registered. Registering a port at the moment simply means giving it a
	// unique ID, determined by this variable here.
	size_t last_port_id = 0;

	// memory manager for envelopes
	slab_memory<envelope_type> __mem_envelopes;

	// typedefs for array of functions below
	typedef bool (*cmp_delivery_fn)(const envelope_type *left, const envelope_type *right);
	typedef bool (*check_delivery_fn)(const envelope_type *e, const iteration_state *state);

	// All hail to the uglyness!
	//
	// The following array contains lambdas which are used to compare the
	// delivery time of two messages during insertion of new messages into the
	// buffer. The comparison depends on the time-mode of the transport. For
	// TTM_NONE, the comparison evaluates to true in all cases, which means that
	// new messages will be appended to the buffer no matter any timing
	// information. In the case of TTM_TICK, comparisons will happend based on
	// delivery_tick, while TTM_TIME leads to comparison relative to
	// delivery_time.
	//
	// TODO: auto-generate the stuff below using X-Macro Lists
	//
	constexpr static cmp_delivery_fn
	__cmp_delivery_fns[TRANSPORT_TIME_MODE_COUNT] =
	{
		// TTM_NONE
		[](const envelope_type *, const envelope_type *) -> bool {
			return true;
		},
		// TTM_TICK
		[](const envelope_type *left, const envelope_type *right) -> bool {
			return left->header.delivery_tick <= right->header.delivery_tick;
		},
		// TTM_TIME
		[](const envelope_type *left, const envelope_type *right) -> bool {
			return left->header.delivery_time <= right->header.delivery_time;
		},
	};

	// The following array contains lambdas which are evaluated to determine if
	// an envelope contains a message that must be delivered. Delivery depends
	// on the transport's time_mode. In case of TTM_NONE, delivery will always
	// happen immediately. In case of TTM_TICK or TTM_TIME, delivery depends on
	// the current tick/timestamp of the transport itself as well as the
	// designated delivery time of the message.
	constexpr static check_delivery_fn
	__check_delivery_fns[TRANSPORT_TIME_MODE_COUNT] =
	{
		// TTM_NONE
		[](const envelope_type *, const iteration_state *) -> bool {
			return true;
		},
		// TTM_TICK
		[](const envelope_type *e, const iteration_state *state) -> bool {
			return e->header.delivery_tick <= state->ticks;
		},
		// TTM_TIME
		[](const envelope_type *e, const iteration_state *state) -> bool {
			return e->header.delivery_time <= state->t;
		},
	};

};


/*
 * alloc_envelope - allocate memory for an envelope
 */
template <typename T>
transport_index_t
alloc_envelope(transport_t<T> &transport)
{
	// TODO: check if value exists
	return transport.__mem_envelopes.alloc().value();
}

/*
 * alloc_envelope - allocate memory for an envelope
 */
template <typename T>
transport_index_t
alloc_envelope(transport_t<T> *transport)
{
	return alloc_envelope(*transport);
}

/*
 * free_envelope - release memory acquired for an envelope
 */
template <typename T>
void free_envelope(
		transport_t<T> &transport,
		const transport_index_t env_index)
{
	transport.__mem_envelopes.free(env_index);
}


/*
 * free_envelope - release memory acquired for an envelope
 */
template <typename T>
void free_envelope(
		transport_t<T> *transport,
		const transport_index_t env_index)
{
	free_envelope(*transport, env_index);
}


/*
 * transport_get_envelope - get a reference to an envelope
 *
 * TODO: check value returned by __mem_envelopes.get(), and maybe also return an
 * optional here
 */
template <typename T>
typename transport_t<T>::envelope_type*
transport_get_envelope(
		transport_t<T> *transport,
		const transport_index_t env_index)
{
	return transport->__mem_envelopes.get(env_index).value();
}


/*
 * connect - connect a source and sink using a given transport via port IDs
 */
template <typename T>
void connect(
		transport_t<T> &transport,
		std::optional<port_index_t> source_id,
		std::optional<port_index_t> sink_id)
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
		transport_t<T> &transport,
		port_t<T> *source,
		port_t<T> *sink)
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
		transport_t<T> &transport,
		std::optional<port_index_t> source_id,
		std::optional<port_index_t> sink_id,
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
		transport_t<T> &transport,
		port_t<T> *source,
		port_t<T> *sink,
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
		transport_t<T> &transport,
		std::optional<port_index_t> source_id,
		std::optional<port_index_t> sink_id)
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
	transport_t<T> &transport,
	std::optional<port_index_t> sink_id)
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
		transport_t<T> &transport,
		port_t<T>* source,
		port_t<T>* sink)
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
		transport_t<T> &transport,
		std::optional<port_index_t> source_id,
		std::optional<port_index_t> sink_id,
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
		transport_t<T> &transport,
		port_t<T> *source,
		port_t<T> *sink,
		Ts... sinks) requires (std::same_as<decltype(sink), Ts> && ...)
{
	disconnect(transport, source->index, sink->index);
	disconnect(transport, source, sinks...);
}


template <typename T>
port_t<T>*
__get_port(transport_t<T> &transport, port_index_t id)
{
	auto needle = transport.known_ports.find(id);
	if (needle != transport.known_ports.end())
		return needle->second;
	return nullptr;
}


// TODO: rename this function
template <typename T>
void
process_messages(
		transport_t<T> &transport,
		const iteration_state *iter_state = nullptr)
{
	// update the transport's iter_state
	transport.iter_state = iter_state;

	// select the proper function to use for time evaluation
	auto check_delivery_fn = transport.__check_delivery_fns[transport.time_mode];

	// mailbuffer is sorted by timestamp, so we can actually break as soon as we
	// reached a message which doesn't need delivery
	for (auto it = transport.buffer.begin(); it != transport.buffer.end();) {

		// the envelope ID is simply what is stored in the buffer
		transport_index_t env_id = *it;

		// TODO: move into a function, also check if the value actually exists
		const envelope_t<T> *env = transport.__mem_envelopes.get(env_id).value();

		if (check_delivery_fn(env, transport.iter_state)) {
			// try to find the port. The port, and thereby its pointer, might
			// have gone away or become invalid in the meantime, so we try to
			// locate it in the list of known ports
			port_t<T> *sink = __get_port(transport, env->id.sink);
			if (sink != nullptr)
				// in this case, the recipient must delete all envelopes
				sink->buffer.push_back(env_id);
			else
				// delete memory allocated for the message
				free_envelope(transport, env_id);

			// remove from list and go to the next item
			it = transport.buffer.erase(it);
		}
		else {
			break;
		}
	}
}


template <typename T>
void
__mailbuffer_insert(
		transport_t<T> &transport,
		const transport_index_t env_id)
{
	// select the proper function for comparison depending on time mode
	auto cmp_delivery_fn = transport.__cmp_delivery_fns[transport.time_mode];

	// assume that new mails will be mostly appended to the list. still, they
	// should be added at the correct location. Hence, we search from the back
	// until we find the proper slot to insert the element
	auto rbegin = transport.buffer.rbegin();
	auto rend   = transport.buffer.rend();

	// TODO: replace calls to __mem_envelopes.get with nice function
	auto new_envelope = transport.__mem_envelopes.get(env_id).value();

	while (rbegin != rend) {
		// left is a pointer to an envelope (get returns an optional)
		auto left = transport.__mem_envelopes.get(*rbegin).value();
		if (cmp_delivery_fn(left, new_envelope))
			break;
		++rbegin;
	}

	transport.buffer.insert(rbegin.base(), env_id);
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
		transport_t<T> &transport,
		std::optional<port_index_t> source_id,
		std::optional<port_index_t> sink_id,
		T payload,
		transport_options_t opts)
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
	transport_index_t env_id = alloc_envelope(transport);
	auto *envelope = transport.__mem_envelopes.get(env_id).value();

	// fill ID fields
	envelope->id.source = source_id.value();
	envelope->id.sink   = sink_id.value();
	envelope->id.msg    = source->last_msg_id++;

	// fill in timing information in the header depending on the transport's
	// time mode
	envelope->header.time_mode = transport.time_mode;
	switch(transport.time_mode) {
	case TTM_TICK:
		envelope->header.ticks =
			transport.iter_state != nullptr
			? transport.iter_state->ticks
			: 0;
		envelope->header.ticks_delayed
			= opts.delay_ticks;
		envelope->header.delivery_tick
			= envelope->header.ticks
			+ envelope->header.ticks_delayed;
		break;

	case TTM_TIME:
		envelope->header.timestamp =
			transport.iter_state != nullptr
			? transport.iter_state->t
			: 0;
		envelope->header.time_delayed
			= opts.delay_time;
		envelope->header.delivery_time
			= envelope->header.timestamp
			+ envelope->header.time_delayed;
		break;

	case TTM_NONE:
	default:
		envelope->header.ticks = 0;
		envelope->header.ticks_delayed = 0;
		envelope->header.delivery_tick = 0;
		break;
	}

	// finally move payload resource
	// TODO: is the move really the right thing to do here? Probably it's better
	//       to copy the payload, and clearly state in the documentation that
	//       this is a copying transport. Hence, it's recommended to send around
	//       only small objects.
	envelope->payload = std::move(payload);

	// in case the transport operates in TTM_NONE, directly put the message into
	// the recipient's buffer. If not, then need to place it into the mail
	// buffer for delivery at the appropriate time.
	// TODO: do the same in case the message is due *right now* in any of the
	//       time modes
	if (transport.time_mode == TTM_NONE) {
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
		transport_t<T> &transport,
		std::optional<port_index_t> source_id,
		T payload,
		transport_options_t opts)
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
		transport_t<T> *transport,
		std::optional<port_index_t> source_id,
		T payload,
		transport_options_t opts)
{
	broadcast<T>(*transport, source_id, payload, opts);
}


template <typename T>
inline void
broadcast(
		transport_t<T> &transport,
		port_t<T> *source,
		T payload,
		transport_options_t opts)
{
	assert(source);
	broadcast<T>(transport, source->index, payload, opts);
}


template <typename T>
inline void
broadcast(
		transport_t<T> *transport,
		port_t<T> *source,
		T payload,
		transport_options_t opts)
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
		transport_t<T> &transport,
		port_t<T> *port)
{
	assert(port);

	// assign a new ID to this port
	port->transport   = &transport;
	port->index       = transport.last_port_id++;
	port->last_msg_id = 0;

	// store the ID in the transport's list of known ports
	transport.known_ports[port->index.value()] = port;
}


/*
 * unregister_port - remove ports from the given transport
 */
template <typename T>
inline void
unregister_ports(
		transport_t<T> &transport,
		port_t<T> *port)
{
	assert(port);
	if (!port->index) return;

	// disconnect this port from everything in the transport system
	disconnect(transport, port->index);

	// remove from known ports
	transport.known_ports.erase(port->index.value());

	// reset the transport pointer, and set index to an unknown state.
	// TODO: could also use optional for the transport
	port->transport = nullptr;
	port->index = {};
}


/*
 * register_ports - make several ports aware to a transport
 */
template <typename T, typename... Ts>
inline void
register_ports(
		transport_t<T> &transport,
		port_t<T> *port,
		Ts... ports) requires (std::same_as<decltype(port), Ts> && ...)
{
	register_ports(transport, port);
	register_ports(transport, ports...);
}


/*
 * register_ports - make several ports aware to a transport
 */
template <typename T, typename... Ts>
inline void
register_ports(
		transport_t<T> *transport,
		port_t<T> *port,
		Ts... ports) requires (std::same_as<decltype(port), Ts> && ...)
{
	register_ports(*transport, port);
	register_ports(*transport, ports...);
}


template <typename T, typename... Ts>
inline void
unregister_ports(
		transport_t<T> &transport,
		port_t<T> *port,
		Ts... ports) requires (std::same_as<decltype(port), Ts> && ...)
{
	unregister_ports(transport, port);
	unregister_ports(transport, ports...);
}


template <typename T, typename... Ts>
inline void
unregister_ports(
		transport_t<T> *transport,
		port_t<T> *port,
		Ts... ports) requires (std::same_as<decltype(port), Ts> && ...)
{
	unregister_ports(*transport, port);
	unregister_ports(*transport, ports...);
}


} // ncr::

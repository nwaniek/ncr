#include <iostream>

#include <ncr/ncr_chrono.hpp>
#include <ncr/ncr_transport2.hpp>
#include <ncr/ncr_simulation.hpp>

using namespace ncr;


// custom payload, must be copyable
struct payload_t {
	double value;
};


void
test_tick_mode()
{
	// custom transport options based on time / clock mode
	struct T0Options {
		ncr::time_point<ClockType::Ticks> delivery_time;
	};

	// traits required for the transport itself
	struct T0TypeTraits {
		using payload_type = payload_t;
		using options_type = T0Options;
	};

	// the transport we want to use
	transport<T0TypeTraits> transport(
			// transport requires a function to compare envelopes, in order to
			// sort them internally into internall message buffers according to
			// their point of delivery
			[](auto &left, auto &right) -> bool {
				return left.options.delivery_time <= right.options.delivery_time;
			}
	);

	using port_type     = decltype(transport)::port_type;
	using envelope_type = decltype(transport)::envelope_type;

	// payload
	payload_t payload{.value = 17.71};

	// additional stuff
	// .delay = ncr::Duration<size_t, 10>;
	//                        ^^^^^^ make this depend on some time mode so
	//                               auto-deduce it by the compiler
	//
	// Need a "time provider" -> can

	// declare some ports and register them
	port_type one, two, three, four, five;
	register_ports(transport, &one, &two, &three, &four, &five);

	// connect the ports. this forms a DAG from `one` to all other ports.
	// Note that, internally, connect also stores reverse pointers for fast
	// backwards traversal.
	connect(transport, &one, &two, &three, &four, &five);

	// disconnect prunes away the edges in the DAG connected source from the
	// list of sinks
#if 1
	disconnect(transport, &one, &three, &five);
#endif

	// send some message to all recipients
	broadcast(transport, &one, payload, {.delivery_time = 5});

	const size_t max_ticks = 10;
	for (size_t ticks = 0; ticks < max_ticks; ++ticks) {
		using namespace std;

		cout << "tick " << ticks << " | ";

		// process all pending mail
		process_messages(
				// process messages needs the transport to process...
				transport,
				// and a function that evaluates if delivery of a certain
				// envelope is possible or not. In the example here, this checks
				// the delivery ticks
				[&ticks](const envelope_type &env) -> bool {
					return env.options.delivery_time <= ticks;
				}
		);

		// TODO: send additional messages or so

		// let's see if the mailboxes of two and three are filled
		cout << "two: " << two.buffer.size() << ", "
		     << "three: " << three.buffer.size() << ", "
		     << "four: " << four.buffer.size() << ", "
		     << "five: " << five.buffer.size() << "\n";
	}

	// four and five got messages. because they received copies, modifications
	// will not affect the state of the other.

	auto envelope = transport_get_envelope(&transport, two.buffer[0]);
	auto &_two = envelope->payload;

	std::cout << "before increment: two.payload.value = " << _two.value << "\n";

	// do a nasty const-cast to cast away the constness of the pointer
	const_cast<payload_t&>(_two).value += 10;
	std::cout << "after increment: two.payload.value = " << _two.value << "\n";

	envelope = transport_get_envelope(&transport, four.buffer[0]);
	auto &_four = envelope->payload;
	std::cout << "four.payload.value = " << _four.value << "\n";

	// delete messages
	port_clear_buffers(two, three, four, five);
}

void
test_time_mode()
{
	struct T1Options {
		ncr::time_point<ClockType::Time> delivery_time;
	};

	struct T1TypeTraits {
		using payload_type = payload_t;
		using options_type = T1Options;
	};

	// the transport we want to use
	transport<T1TypeTraits> transport(
			// transport requires a function to compare envelopes, in order to
			// sort them internally into internall message buffers according to
			// their point of delivery
			[](auto &left, auto &right) -> bool {
				return left.options.delivery_time <= right.options.delivery_time;
			}
	);

	// payload
	payload_t payload{ .value = 17.71};

	using port_type     = decltype(transport)::port_type;
	using envelope_type = decltype(transport)::envelope_type;

	// declare some ports and register them (mandatory)
	port_type one, two, three, four, five;
	register_ports(transport, &one, &two, &three, &four, &five);

	// connect the ports. this forms a DAG from `one` to all other ports.
	// Note that, internally, connect also stores reverse pointers for fast
	// backwards traversal.
	connect(transport, &one, &two, &three, &four, &five);

	// disconnect prunes away the edges in the DAG connected source from the
	// list of sinks
#if 0
	disconnect(&one, {&three, &five});
#endif
	// local iteration information
	iteration_state iter_state{
		.dt    =  1.0_ms,
		.t_0   =  0.0_ms,
		.t_max = 20.0_ms,
		.t     =  0.0_ms,
		.ticks =  0,
		.timeless = false,
	};

	// send some message to all recipients
	broadcast(transport, &one, payload, {.delivery_time = 7.0_ms});
	// send another message
	broadcast(transport, &one, payload, {.delivery_time = 14.0_ms});

	while (iter_state.t < iter_state.t_max) {
		using namespace std;

		cout << "time " << iter_state.t << " | ";

		// process all pending mail
		process_messages(
				transport,
				[&iter_state](const envelope_type &env) -> bool {
					return env.options.delivery_time <= iter_state.t;
				}
		);

		// let's see if the mailboxes of two and three are filled
		cout << "two: " << two.buffer.size() << ", "
		     << "three: " << three.buffer.size() << ", "
		     << "four: " << four.buffer.size() << ", "
		     << "five: " << five.buffer.size() << "\n";

		// advance time
		iter_state.t += iter_state.dt;
	}

	// four and five got messages. because they received copies, modifications
	// will not affect the state of the other.
	auto envelope = transport_get_envelope(&transport, two.buffer[0]);
	auto &_two = envelope->payload;
	std::cout << "before two.payload.value = " << _two.value << "\n";

	// again the nasty const cast
	const_cast<payload_t&>(_two).value += 10;
	std::cout << "after two.payload.value = " << _two.value << "\n";

	envelope = transport_get_envelope(&transport, four.buffer[0]);
	auto &_four = envelope->payload;
	std::cout << "four.payload.value = " << _four.value << "\n";

	// delete messages
	port_clear_buffers(two, three, four, five);
}




void
test_notime_mode()
{
	struct T2TypeTraits {
		using payload_type = payload_t;
		using options_type = null_options;
	};

	// the transport we want to use
	using transport_type = transport<T2TypeTraits>;
	using port_type      = transport_type::port_type;

	transport_type transport(transport_type::back_inserter{});

	payload_t payload;
	payload.value = 1234.0;

	// declare some ports, and register them (mandatory)
	port_type one, two, three, four, five;
	register_ports(transport, &one, &two, &three, &four, &five);

	// connect the ports. this forms a DAG from `one` to all other ports.
	// Note that, internally, connect also stores reverse pointers for fast
	// backwards traversal.
	connect(transport, &one, &two, &three, &four, &five);

	// disconnect prunes away the edges in the DAG connected source from the
	// list of sinks
#if 0
	disconnect(&one, {&three, &five});
#endif
	// local iteration information
	iteration_state iter_state{
		.dt    =  1.0_ms,
		.t_0   =  0.0_ms,
		.t_max = 10.0_ms,
		.t     =  0.0_ms,
		.ticks =  0,
		.timeless = false,
	};

	// send some message to all recipients
	broadcast(transport, &one, payload, {});

	while (iter_state.t < iter_state.t_max) {
		using namespace std;

		cout << "time " << iter_state.t << " | ";

		// default behavior in this example is to simply accept all envelopes.
		process_messages(transport, transport.accept_all);

		// let's see if the mailboxes of two and three are filled
		cout << "two: " << two.buffer.size() << ", "
		     << "three: " << three.buffer.size() << ", "
		     << "four: " << four.buffer.size() << ", "
		     << "five: " << five.buffer.size() << "\n";

		// advance time
		iter_state.t += iter_state.dt;
	}

	// four and five got messages. because they received copies, modifications
	// will not affect the state of the other.
	auto envelope = transport_get_envelope(&transport, two.buffer[0]);
	auto &_two = envelope->payload;
	std::cout << "before two.payload.value = " << _two.value << "\n";

	// again the nasty const cast
	const_cast<payload_t&>(_two).value += 10;
	std::cout << "after two.payload.value = " << _two.value << "\n";

	envelope = transport_get_envelope(&transport, four.buffer[0]);
	auto &_four = envelope->payload;
	std::cout << "four.payload.value = " << _four.value << "\n";

	// tidy up
	port_clear_buffers(two, three, four, five);
}
/*


void
test_pointers()
{
	// the transport we want to use
	transport_t<payload_t> transport;
	transport.time_mode = TTM_TIME;

	// payload
	payload_t payload{ .value = 17.71};

	// declare some ports and register them (mandatory)
	port_t<payload_t> *one, *two, *three, *four, *five;

	one   = new port_t<payload_t>();
	two   = new port_t<payload_t>();
	three = new port_t<payload_t>();
	four  = new port_t<payload_t>();
	five  = new port_t<payload_t>();

	register_ports(transport, one, two, three, four, five);


	// connect the ports. this forms a DAG from `one` to all other ports.
	// Note that, internally, connect also stores reverse pointers for fast
	// backwards traversal.
	connect(transport, one, two, three, four, five);

	// disconnect prunes away the edges in the DAG connected source from the
	// list of sinks
#if 1
	disconnect(transport, one, three, five);
	delete five;
	five = nullptr;
#endif

	// local iteration information
	iteration_state iter_state{
		.dt    =  1.0_ms,
		.t_0   =  0.0_ms,
		.t_max = 20.0_ms,
		.t     =  0.0_ms,
		.ticks =  0,
	};
	// force transport to know about the iteration state
	transport.iter_state = &iter_state;

	// send some message to all recipients
	transport_options_t opts{ .delay_time = 7.0_ms };
	broadcast(transport, one, payload, opts);

	// send another message
	opts.delay_time = 14.0_ms;
	broadcast(transport, one, payload, opts);

	while (iter_state.t < iter_state.t_max) {
		using namespace std;

		cout << "time " << iter_state.t << " | ";

		// process all pending mail
		process_messages(transport, &iter_state);

		// let's see if the mailboxes of two and three are filled
		cout << "two: " << two->buffer.size() << ", "
		     << "three: " << three->buffer.size() << ", "
		     << "four: " << four->buffer.size() << "\n";
		     // << "five: " << five->buffer.size() << "\n";

		// advance time
		iter_state.t += iter_state.dt;
	}

	// four and five got messages. because they received copies, modifications
	// will not affect the state of the other.
	auto envelope = transport_get_envelope(&transport, two->buffer[0]);
	auto &_two = envelope->payload;
	std::cout << "before two.payload.value = " << _two.value << "\n";

	// again the nasty const cast
	const_cast<payload_t&>(_two).value += 10;
	std::cout << "after two.payload.value = " << _two.value << "\n";

	envelope = transport_get_envelope(&transport, four->buffer[0]);
	auto &_four = envelope->payload;
	std::cout << "four.payload.value = " << _four.value << "\n";

	// delete messages
	port_clear_buffers(*two, *four);

	delete one;
	delete two;
	delete three;
	delete four;
	// five got deleted ahead in the #if-part
}
*/


int
main()
{
	std::cout << "tick mode\n";
	test_tick_mode();

	std::cout << "\ntime mode\n";
	test_time_mode();

	std::cout << "\nmode ignoring time\n";
	test_notime_mode();

	/*
	std::cout << "\npointer test\n";
	test_pointers();
	*/

	return 0;
}

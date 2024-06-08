#include <iostream>
#include <vector>
#include <cmath>
#include <cstddef>
#include <cassert>
#include <iomanip> // setprecision
#include <algorithm>
#include <fstream>

#include "shared.hpp"
#include <ncr/ncr_log.hpp>
#include <ncr/ncr_math.hpp>
#include <ncr/ncr_numeric.hpp>
#include <ncr/ncr_neuron.hpp>
#include <ncr/ncr_utils.hpp>
#include <ncr/ncr_units.hpp>
#include <ncr/ncr_random.hpp>
#include <ncr/ncr_algorithm.hpp>

#include <cblas.h>

#define COUT_PRECISION 4

// TODO: don't do this
using namespace ncr;

namespace ncr {
	NCR_LOG_DECLARATION;
}



void
test_izhikevich_neuron(std::string type = "tonic_spiking")
{
	Izhikevich::Neuron n = Izhikevich::make(type);
	auto input = Izhikevich::get_demo_input(type);
	// the input is simply defined as:
	//    [](double t) -> double { if (t > 10.0_ms) return 14.0_mV; return 0.0; };

	double t    =   0.0_ms;
	double dt   =   0.1_ms;
	double tmax = 500.0_ms;

	std::ofstream outputfile;
	outputfile.open("visualize_izhikevich_new.py");

	outputfile << "#!/usr/bin/env python\n\n";

	outputfile << "import numpy as np\n";
	outputfile << "import matplotlib.pyplot as plt\n\n";

	outputfile << "data = np.array([[" << t << ", " << n.state.v_reported << "]";
	while (t < tmax) {
		// this updates both t and dt
		Izhikevich::step(n, t, dt, input);
		// print data
		outputfile << ", [" << t << ", " << n.state.v_reported << "]";
	}
	outputfile << "])\n\n";

	outputfile << "plt.figure()\n";
	outputfile << "plt.plot(data.T[0,:], data.T[1,:])\n";
	outputfile << "plt.xlabel('ms')\n";
	outputfile << "plt.ylabel('mV')\n";
	outputfile << "plt.title('Spike train of Izhikevich Neuron type \"" << type << "\"')\n";
	outputfile << "plt.show()\n";

	outputfile.close();

}


#if 0
static std::mt19937_64 *__rng = nullptr;
std::normal_distribution<double> normdist10(0.0, 10.0);
std::normal_distribution<double> normdist2(0.0, 4.0);

template <typename T = double>
T
izhikevich_population_demo_input(T t, void*)
{
	bool gen_input = false;

	/*
	gen_input =
		   (t > 10.0_ms && t < 200.0_s)
		|| (t > 400.0_s && t < 500.0_s)
	;
	*/
	// gen_input = (t > 100.0_ms && t < 100.0_s) || (t > 200.0_s && t < 400.0_s);
	gen_input =
		   (t >  10.0_ms)
		;

	if (!gen_input)
		return ncr::max(0.0, normdist2(*__rng));
	return ncr::max(0.0, 4.0 + normdist10(*__rng));
}

void
test_izhikevich_population()
{
	__rng = mkrng();


	constexpr size_t N = 250;
	constexpr char type[] = "phasic_bursting";

	std::vector<IzhikevichNeuron<double>*> neurons;
	for (size_t i = 0; i < N; i++) {
		neurons.push_back(new IzhikevichNeuron<double>(type));
		neurons[i]->set_input(izhikevich_population_demo_input<double>);
	}

	double t    =    0.0_ms;
	double dt   =    0.1_ms;
	double tmax =    1.0_s;

	std::ofstream outputfile;
	outputfile.open("visualize_izhikevich_population.py");

	outputfile << "#!/usr/bin/env python\n\n";

	outputfile << "import numpy as np\n";
	outputfile << "import matplotlib.pyplot as plt\n";
	outputfile << "from rasterplotter import rasterplot\n";

	outputfile << "dt = " << dt << "\n";
	outputfile << "data = np.array([\n";

	while (t < tmax) {

		// step each neuron forward
		size_t i = 0;

		// double t_next = t;
		double t_next = t;
		for (auto n : neurons) {

			// step each neuron forward using the integrate method. Also store
			// the update time (for numerical reasons, use max, and assign the
			// value later to t
			t_next = ncr::max(n->integrate(t, dt), t_next);

			if (i++)
				outputfile << ", ";
			else
				outputfile << "        [";
			outputfile << n->state.spiking;
		}
		outputfile << "], \n";

		t = t_next;
	}
	outputfile << "])\n";
	outputfile << "data = data.T\n\n";

	outputfile << "fig = plt.plot()\n";
	outputfile << "rasterplot(data, dt=dt)\n";
	outputfile << "plt.show()\n";

	outputfile.close();


	for (size_t i = 0; i < N; i++) {
		delete neurons[i];
	}

	delete __rng;
}
#endif


void
test_fitzhughnagumo_neuron()
{
	FitzhughNagumo::Neuron n = FitzhughNagumo::make();
	auto input = FitzhughNagumo::get_demo_input();

	double t    =   0.0_ms;
	double dt   =   0.1_ms;
	double tmax = 500.0_ms;

	std::ofstream outputfile;
	outputfile.open("visualize_fitzhughnagumo.py");

	outputfile << "#!/usr/bin/env python\n\n";

	outputfile << "import numpy as np\n";
	outputfile << "import matplotlib.pyplot as plt\n\n";

	outputfile << "data = np.array([[" << t << ", " << n.state.v[0] << "]";

	while (t < tmax) {
		// this updates both t and dt
		FitzhughNagumo::step(n, t, dt, input);
		// print data
		outputfile << ", [" << t << ", " << n.state.v[0] << "]";
	}
	outputfile << "])\n\n";

	outputfile << "plt.figure()\n";
	outputfile << "plt.plot(data.T[0,:], data.T[1,:])\n";
	outputfile << "plt.xlabel('ms')\n";
	outputfile << "plt.ylabel('mV')\n";
	outputfile << "plt.title('FitzhughNagumoNeuron')\n";
	outputfile << "plt.show()\n";

	outputfile.close();
}


// TODO: maybe add "Neuron" into all class names, i.e. AdExNeuronState instead
//       of AdExState?


void
test_adexif_neuron()
{
	AdEx::Neuron n = AdEx::make();
	auto input = AdEx::get_demo_input();

	double t    =   0.0_ms;
	double dt   =   0.1_ms;
	double tmax = 500.0_ms;

	std::ofstream outputfile;
	outputfile.open("visualize_adex.py");

	outputfile << "#!/usr/bin/env python\n\n";

	outputfile << "import numpy as np\n";
	outputfile << "import matplotlib.pyplot as plt\n\n";

	outputfile << "data = np.array([[" << t << ", " << n.state.v[0] << "]";

	while (t < tmax) {
		// this updates both t and dt
		AdEx::step(n, t, dt, input);
		// print data
		outputfile << ", [" << t << ", " << n.state.v[0] << "]";
	}
	outputfile << "])\n\n";

	outputfile << "plt.figure()\n";
	outputfile << "plt.plot(data.T[0,:], data.T[1,:])\n";
	outputfile << "plt.xlabel('ms')\n";
	outputfile << "plt.ylabel('mV')\n";
	outputfile << "plt.title('AdEx Neuron')\n";
	outputfile << "plt.show()\n";

	outputfile.close();
}



void
test_adexqif_neuron()
{
	AdExQuadratic::Neuron n = AdExQuadratic::make();
	auto input = AdExQuadratic::get_demo_input();

	double t    =   0.0_ms;
	double dt   =   0.1_ms;
	double tmax = 500.0_ms;

	std::ofstream outputfile;
	outputfile.open("visualize_adexquadratic.py");

	outputfile << "#!/usr/bin/env python\n\n";

	outputfile << "import numpy as np\n";
	outputfile << "import matplotlib.pyplot as plt\n\n";

	outputfile << "data = np.array([[" << t << ", " << n.state.v[0] << "]";

	while (t < tmax) {
		// this updates both t and dt
		AdExQuadratic::step(n, t, dt, input);
		// print data
		outputfile << ", [" << t << ", " << n.state.v[0] << "]";
	}
	outputfile << "])\n\n";

	outputfile << "plt.figure()\n";
	outputfile << "plt.plot(data.T[0,:], data.T[1,:])\n";
	outputfile << "plt.xlabel('ms')\n";
	outputfile << "plt.ylabel('mV')\n";
	outputfile << "plt.title('Quadratic AdEx Neuron')\n";
	outputfile << "plt.show()\n";

	outputfile.close();
}


void
test_leakyif_neuron()
{
	LeakyIF::Neuron n = LeakyIF::make();
	auto input = LeakyIF::get_demo_input();

	double t    =   0.0_ms;
	double dt   =   0.1_ms;
	double tmax = 500.0_ms;

	std::ofstream outputfile;
	outputfile.open("visualize_leakyif.py");

	outputfile << "#!/usr/bin/env python\n\n";

	outputfile << "import numpy as np\n";
	outputfile << "import matplotlib.pyplot as plt\n\n";

	outputfile << "data = np.array([[" << t << ", " << n.state.v[0] << "]";

	while (t < tmax) {
		// this updates both t and dt
		LeakyIF::step(n, t, dt, input);
		// print data
		outputfile << ", [" << t << ", " << n.state.v[0] << "]";
	}
	outputfile << "])\n\n";

	outputfile << "plt.figure()\n";
	outputfile << "plt.plot(data.T[0,:], data.T[1,:])\n";
	outputfile << "plt.xlabel('ms')\n";
	outputfile << "plt.ylabel('mV')\n";
	outputfile << "plt.title('Leaky Integrate and Fire Neuron')\n";
	outputfile << "plt.show()\n";

	outputfile.close();
}


void
test_quadraticif_neuron()
{
	QuadraticIF::Neuron n = QuadraticIF::make();
	auto input = QuadraticIF::get_demo_input();

	double t    =   0.0_ms;
	double dt   =   0.1_ms;
	double tmax = 500.0_ms;

	std::ofstream outputfile;
	outputfile.open("visualize_quadraticif.py");

	outputfile << "#!/usr/bin/env python\n\n";

	outputfile << "import numpy as np\n";
	outputfile << "import matplotlib.pyplot as plt\n\n";

	outputfile << "data = np.array([[" << t << ", " << n.state.v[0] << "]";

	while (t < tmax) {
		// this updates both t and dt
		QuadraticIF::step(n, t, dt, input);
		// print data
		outputfile << ", [" << t << ", " << n.state.v[0] << "]";
	}
	outputfile << "])\n\n";

	outputfile << "plt.figure()\n";
	outputfile << "plt.plot(data.T[0,:], data.T[1,:])\n";
	outputfile << "plt.xlabel('ms')\n";
	outputfile << "plt.ylabel('mV')\n";
	outputfile << "plt.title('Quadratic Integrate and Fire Neuron')\n";
	outputfile << "plt.show()\n";

	outputfile.close();
}


void
test_hodgkin_huxley_neuron()
{
	HodgkinHuxley::Neuron n = HodgkinHuxley::make("classical");
	auto input = HodgkinHuxley::get_demo_input("classical");

	double t    =   0.0_ms;
	// NOTE: the HH model requires a really small stepsize for the numerical integration
	double dt   =   0.01_ms;
	double tmax = 500.0_ms;

	std::ofstream outputfile;
	outputfile.open("visualize_hodgkinhuxley.py");

	outputfile << "#!/usr/bin/env python\n\n";

	outputfile << "import numpy as np\n";
	outputfile << "import matplotlib.pyplot as plt\n\n";

	outputfile << "data = np.array([[" << t << ", " << n.state.v[0] << ", " << n.state.v[1]  << ", " << n.state.v[2] << ", " <<  n.state.v[3]<< "]\n";

	while (t < tmax) {
		// this updates both t and dt, but we always want to integrate dt time
		// forward. We thus need to pass in a "locally fixed" dt (tmp_dt).
		double tmp_dt = dt;
		t = HodgkinHuxley::integrate(n, t, tmp_dt, input);

		// print data
		outputfile << "    , [" << t << ", " << n.state.v[0] << ", " << n.state.v[1]  << ", " << n.state.v[2] << ", " <<  n.state.v[3]<< "]\n";
	}
	outputfile << "])\n\n";

	outputfile << "plt.figure()\n";
	outputfile << "plt.plot(data.T[0,:], data.T[1,:])\n";
	outputfile << "plt.xlabel('ms')\n";
	outputfile << "plt.ylabel('mV')\n";
	outputfile << "plt.title('Hodgkin Huxley Neuron')\n";
	outputfile << "plt.show()\n";

	outputfile.close();
}


#if 0
void
test_generalizedif_neuron()
{
	GeneralizedIFNeuron neuron;
	neuron.set_input(generalizedif_demo_input, nullptr);

	double t    =   0.0_ms;
	// NOTE: the HH model requires a really small stepsize for the numerical integration
	double dt   =   0.01_ms;
	double tmax = 500.0_ms;

	std::ofstream outputfile;
	outputfile.open("visualize_generalizedif.py");

	outputfile << "#!/usr/bin/env python\n\n";

	outputfile << "import numpy as np\n";
	outputfile << "import matplotlib.pyplot as plt\n\n";

	outputfile << "data = np.array([[" << t << ", " << neuron.state.v[0] << ", " << neuron.state.v[1]  << ", " << neuron.state.v[2] << ", " <<  neuron.state.v[3]<< "]\n";

	while (t < tmax) {

		// this updates both t and dt, but we always want to integrate dt time
		// forward. We thus need to pass in a "locally fixed" dt (tmp_dt).
		double tmp_dt = dt;
		t = neuron.integrate(t, tmp_dt);

		// print data
		outputfile << "    , [" << t << ", " << neuron.state.v[0] << ", " << neuron.state.v[1]  << ", " << neuron.state.v[2] << ", " <<  neuron.state.v[3]<< "]\n";
	}
	outputfile << "])\n\n";

	outputfile << "plt.figure()\n";
	outputfile << "plt.plot(data.T[0,:], data.T[1,:])\n";
	outputfile << "plt.xlabel('ms')\n";
	outputfile << "plt.ylabel('mV')\n";
	outputfile << "plt.title('Generalized IF Neuron')\n";
	outputfile << "plt.show()\n";

	outputfile.close();
}
#endif

void
test_stdp_kernel()
{
	// see ~/dev/0/theta_stdp_decorrelation for some details
}





int
main(int argc, char *argv[])
{
	NCR_UNUSED(argc, argv);
	NCR_LOG_INSTANCE.set_policy(new logger_policy_stdcout());

#if 1
	std::string izhikevich_type = "tonic_spiking";
	// // if (argc > 1)
	// // 	izhikevich_type = argv[1];
	test_izhikevich_neuron();
	test_fitzhughnagumo_neuron();
	test_adexif_neuron();
	test_adexqif_neuron();
	test_leakyif_neuron();
	test_quadraticif_neuron();
	// test_hodgkin_huxley_neuron();

#endif

	// test_stdp_kernel();
	// test_izhikevich_population();


	// TODO:
	// test_generalizedif_neuron();
	// ExponentialIF

	return 0;
}

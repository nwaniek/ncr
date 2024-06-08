STD=-std=c++20
#CXX=clang++
#CXX=g++
WARNINGS=-Wall -Wextra -pedantic

INCLUDE=-I./include -I..
CFLAGS=$(STD) $(WARNINGS) $(INCLUDE) -DEBUG -g
#CFLAGS=$(STD) $(WARNINGS) $(INCLUDE) -O2
LDFLAGS=
BLASFLAGS=`pkg-config --libs --cflags blas`

ZLIBFLAGS=`pkg-config --libs --cflags zlib`
ZLIBNGFLAGS=`pkg-config --libs --cflags zlib-ng`
LIBZIPFLAGS=`pkg-config --libs --cflags libzip`

all: test_cmdcvar test_genome test_log test_neurons test_odesolver test_transport test_transport2 \
	test_vector test_alphabet test_random test_hdf5io test_bits test_samplers test_enumclass_operators \
	test_zip

test_transport: src/test_transport.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $<

test_transport2: src/test_transport2.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $<

test_cmdcvar: src/test_cmdcvar.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $<

test_genome: src/test_genome.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $<

test_log: src/test_log.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $<

test_hdf5io: src/test_hdf5io.cpp
	$(CXX) $(CFLAGS) -I./external/HighFive/include -DHIGHFIVE_USE_BOOST=OFF -DHIGHFIVE_USE_EIGEN=OFF $(LDFLAGS) -lhdf5 -o $@ $<

test_bits: src/test_bits.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $<

test_samplers: src/test_samplers.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $<

test_enumclass_operators: src/test_enumclass_operators.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $<

test_zip: src/test_zip.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) $(LIBZIPFLAGS) -o $@ $<




# the first variant is with Intel MKL, the second with OpenBLAS (NOTE: on
# archlinux, this also requires to explicitly link against cblas, because
# openblas on archlinux unfortunately doesn't ship with the cblas_* function
# symbols)
# $(CXX) $(CFLAGS) $(LDFLAGS) `pkg-config --libs --cflags mkl-dynamic-lp64-iomp` -o $@ $<
test_odesolver: src/test_odesolver.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) $(BLASFLAGS) -lopenblas -lcblas -o $@ $<

test_neurons: src/test_neurons.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) $(BLASFLAGS) -o $@ $<

test_vector: src/test_vector.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) $(BLASFLAGS) -o $@ $<

test_alphabet: src/test_alphabet.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $<

test_random: src/test_random.cpp
	$(CXX) $(CFLAGS) $(LDFLAGS) -o $@ $<


# .PHONY: test_log test_odesolver test_neurons


valgrind-test_transport: test_transport
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=$@.txt ./$<

valgrind-test_cmdcvar: test_cmdcvar
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=$@.txt ./$<

valgrind-test_genome: test_genome
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=$@.txt ./$<

valgrind-test_odesolver: test_odesolver
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=$@.txt ./$<

valgrind-test_vector: test_vector 
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=$@.txt ./$<

valgrind-test_alphabet: test_alphabet
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=$@.txt ./$<

valgrind-test_random: test_random
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=$@.txt ./$<

valgrind-test_bits: test_bits
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=$@.txt ./$<

valgrind-test_samplers: test_samplers
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=$@.txt ./$<



clean:
	rm -f test_cmdcvar test_genome test_log test_neurons test_odesolver test_transport test_transport2 test_vector test_alphabet \
		test_random test_hdf5io test_bits test_samplers test_npy test_parser test_npy2 test_parser2 test_enumclass_operators \
		test_zip visualize_adex.py visualize_fitzhughnagumo.py visualize_leakyif.py visualize_adexquadratic.py \
		visualize_izhikevich_new.py visualize_quadraticif.py


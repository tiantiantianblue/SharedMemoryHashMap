CPPFLAGS = -fPIC  -I/home/tian-qing/source_code/boost_1_61_0 -std=c++11
LDFLAGS = -shared  -static-libstdc++ -static-libgcc
libsm_hash_map.so:sm_hash_map.o
	g++ -o libsm_hash_map.so $(LDFLAGS) sm_hash_map.o
clean:
	rm *.o

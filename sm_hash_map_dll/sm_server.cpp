#include"sm_hash_map.h"
#include<string>
#include<iostream>
#include<vector>
#include<boost/progress.hpp>
using namespace std;
int mains()
{
	vector<string> v;
	const size_t size = 50000;
	for (size_t i = 0; i < size; ++i)
		v.push_back(to_string(i));

	SM_HANDLE handle=sm_server_init( "sm2", 128, 1024, size);
	boost::progress_timer t;
	for (int i = 0; i < 200; ++i)
		for (auto cc : v)
			sm_set(handle, cc.c_str(), cc.c_str());

	cout << "sm_avr_depth " << sm_avg_depth(handle) << endl;
	cout << "sm_size " << sm_size(handle) << endl;
	cout << "sm_memory_use " << sm_memory_use(handle) << endl;
	cout << "sm_total_memory " << sm_total_memory(handle) << endl;
	return 0;
}
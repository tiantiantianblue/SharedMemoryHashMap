//for test
#include"sm_hash_map.h"
#include<string>
#include<iostream>
#include<vector>
#include<boost/progress.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include<thread>
#include<mutex>
using namespace std;
using namespace boost::interprocess;
static named_mutex muu(open_or_create, "log");
const size_t siz = 1000*100;
int kk=0;
//const char* name = "ae";
void sg(const char* name)
{
	vector<string> v;
	for (size_t i = 0; i < siz; ++i)
		v.push_back(to_string(i));
	SM_HANDLE handle = sm_server_init(name, 64, 64, siz);
	if (!handle)
		return;
	//boost::progress_timer t;
	for (int i = 0; i < 1; ++i)
		for (auto cc : v)
			sm_set(handle, cc.c_str(), cc.c_str());
	lock_guard<named_mutex> lg(muu);
	cout << endl<<"server" << endl;
	cout << "sm_avr_depth " << sm_avg_depth(handle) << endl;
	cout << "sm_size " << sm_size(handle) << endl;
	cout << "sm_memory_use " << sm_memory_use(handle) << endl;
	cout << "sm_total_memory " << sm_total_memory(handle) << endl;
}

void g(const char* name)
{
	vector<string> v;
	for (int i = 0; i < siz; ++i)
		v.push_back(to_string(i));


	SM_HANDLE handle = sm_server_init(name, 64, 64, siz);
	if (!handle)
		return;
	//boost::progress_timer t;

	int k = 0;
	for (int i = 0; i < 1; ++i)
		for (auto cc : v)
		{
			size_t len = 128;
			char out[128];
			if (sm_get(handle, cc.c_str(), out, len) == 0)
			if (cc != out)
				++k;
		}
	lock_guard<named_mutex> lg(muu);
	cout <<endl<< "client" << endl;
	cout << "error number: " << k << endl;
	cout << "sm_avr_depth " << sm_avg_depth(handle) << endl;
	cout << "sm_size " << sm_size(handle) << endl;
	cout << "sm_memory_use " << sm_memory_use(handle) << endl;
	cout << "sm_total_memory " << sm_total_memory(handle) << endl;
	cout << "sm_key_len" << sm_key_len(handle) << endl;
	cout << "sm_value_len" << sm_value_len(handle) << endl;

}

void x(const string& s)
{
	cout << s << endl;
}
int main()
{
	vector<thread> v;
	const char *ss[] = {"se","sf","sc","sd"};
	const char *xa = "dc";
	int n = 2;
	for (int i = 0; i < n; ++i)
		v.emplace_back(bind(sg, xa));
	for (int i = 0; i < n; ++i)
		v.emplace_back(bind(g, xa));
	for (int i = 0; i < v.size(); ++i)
		v[i].join();
	return 0;
}

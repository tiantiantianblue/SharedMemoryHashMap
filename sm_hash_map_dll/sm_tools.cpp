#include"sm_hash_map.h"
#include<string>
#include<iostream>
#include<vector>
#include<thread>
#include<mutex>
#include<boost/progress.hpp>
#include<boost/algorithm/string.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
using namespace std;
using namespace boost::algorithm;
using namespace boost::interprocess;

void help()
{
	cout << "usage:\n"
		"use [namespace]\n"
		"show: list all namespace\n"
		"get key\n"
		"set key value\n"
		"list [number]\n"
		"info\n" << endl;;
}
static SM_HANDLE handle;
static string trimm(string s)
{
	trim(s);
	return s;
}
int main()
{
	string input;
	while (std::getline(cin, input))
	{
		if (trimm(input) == "show")
		{
			shared_memory_object smo(open_only, "sm_namespaces", read_only);
			mapped_region rg(smo, read_only);
			char *q = static_cast<char*>(rg.get_address());
			while (*q)
			{
				cout << q << endl;
				q += 128;
			}
			continue;
		}

		if (starts_with(input, "use"))
		{
			if (handle)
				sm_release(handle);
			handle = sm_client_init(trimm(input.substr(3)).c_str());

			if (!handle)
			{
				cout << "namespace " << trimm(input.substr(3)) << " not exsit" << endl;
			}
			continue;

		}

		if (input == "h" || input == "help")
		{
			help();
		}

		else if (input == "q" || input == "quit" || input == "exit")
		{
			break;
		}

		else if (!handle)
		{
			cout << "use namespace first!" << endl;
			continue;
		}

		else if (input == "info")
		{
			cout << "sm_avr_depth " << sm_avg_depth(handle)
				<< "\nsm_size " << sm_size(handle) 
				<< "\nsm_memory_use " << sm_memory_use(handle) 
				<< "\nsm_total_memory " << sm_total_memory(handle)
				<< "\nsm_key_len " << sm_key_len(handle)
				<< "\nsm_value_len " << sm_value_len(handle) <<"\n"<< endl;
		}

		else if (starts_with(input, "list"))
		{
			int n;
			if (trimm(input.substr(4)).empty())
				n = 20;
			else
				n = stoi(trimm(input.substr(4)));
			auto head = sm_bucket_head(handle);

			for (size_t i = 0; i < n&&i < sm_size(handle);)
			{
				if (*head)
				{
					cout << "key: " << head << "\n"
						<< "value: " << head + sm_key_len(handle) << "\n";
					++i;
				}
				head += sm_key_len(handle) + sm_value_len(handle) + sizeof size_t;
			}
			cout << endl;
		}

		else if (starts_with(input, "get"))
		{
			string key = trimm(input.substr(3));
			char value[4096];
			size_t len = 4096;
			int error = sm_get(handle, key.c_str(), value, len);
			if (error != 0)
				cout << "sm_get error: " << error << endl;
			else
				cout << value << endl;
		}

		else if (starts_with(input, "set"))
		{
			string kv = trimm(input.substr(3));
			string key = kv.substr(0, kv.find_first_of(" "));
			string value = trimm(kv.substr(kv.find_first_of(" ")));
			sm_set(handle, key.c_str(), value.c_str());
		}

		else
		{
			cout << "command not found" << endl;
		}
	}

}
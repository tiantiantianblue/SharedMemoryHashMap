#define MYDLL

#include"sm_hash_map.h"
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include<memory>
#include<mutex>
#include<iostream>
#include<atomic>
using namespace boost::interprocess;
using namespace std;
const static size_t mutex_number = 97;
const static size_t version = 1019;
struct sm_info
{
	size_t key_size;
	size_t value_size;
	size_t kvi_size;
	size_t bucket_size;
	size_t *used_block;
	size_t *elements;
	size_t *used_bucket;
	size_t *free_block;
	size_t max_block;
	char* bucket_head;
	char* extend_head;
	string name;
	std::shared_ptr<named_mutex> mt_used_block;
	std::shared_ptr<named_mutex> mu_elements;

	vector<std::shared_ptr<named_mutex>> mtx;
	std::shared_ptr<shared_memory_object> shm;
	std::shared_ptr<mapped_region> region;
};

static map<string, std::shared_ptr<shared_memory_object> > shdmems;
static map<string, std::shared_ptr<mapped_region> > regions;
static size_t get_bucket_no(const sm_info* info, const char* key)
{
	return hash<string>()(key) % info->bucket_size;
}

static std::shared_ptr<named_mutex> get_mutex(const sm_info* info, const char* key)
{
	return info->mtx[get_bucket_no(info, key) % mutex_number];
}

static size_t total_memory(std::shared_ptr<shared_memory_object> shm)
{
	offset_t size = 0;
	shm->get_size(size);
	return static_cast<size_t>(size);
}

static void init_mutex(sm_info* info)
{
	info->mu_elements = make_shared<named_mutex>(open_or_create, (info->name + "_elements").c_str());
	info->mt_used_block = make_shared<named_mutex>(open_or_create, (info->name + "_used_block").c_str());
	for (size_t i = 0; i < mutex_number; ++i)
		info->mtx.push_back(make_shared<named_mutex>(open_or_create, (info->name + to_string(i)).c_str()));
}

//三个指针对应桶的三个字段的地址，next指向hash_mod值与该桶元素相同的下一个，没有则为nullptr
struct kvi
{
	struct kvi(const sm_info* info, size_t id_)
	{
		id = id_;
		key = info->bucket_head + info->kvi_size*id_;
		value = key + info->key_size;
		index = reinterpret_cast<size_t*>(value + info->value_size);
		//如果index不为0， 则生成桶链表的下一元素，并链接， 如此便可生成完整的桶链表，链表的最后一个元素index值为0 
		if (*index)
			next = make_shared<kvi>(info, *index);
	}
	char* key;
	char* value;
	size_t* index;
	size_t id;
	std::shared_ptr<kvi> next;
};


static void init2(sm_info* info, size_t* p)
{
	info->elements = ++p;
	info->used_block = ++p;
	if (!*info->used_block)
		*info->used_block = info->bucket_size + 1;
	info->used_bucket = ++p;
	info->free_block = ++p;
	info->bucket_head = reinterpret_cast<char*>(++p);
	info->extend_head = info->bucket_head + info->bucket_size * info->kvi_size;
}

//重复打开会导致进程空间地址增长，然后地址空间溢出崩溃，故进程对一个共享内存只打开一次，多线程引用计数共享该shared_memory_object
static std::shared_ptr<shared_memory_object> get_shm(const char* name)
{
	if (shdmems.find(name) == shdmems.end())
		shdmems[name] = make_shared<shared_memory_object>(open_or_create, name, read_write);
	return shdmems[name];
}

DLL_API SM_HANDLE sm_server_init(const char* name, size_t key, size_t value, size_t num, double factor)
{
	named_mutex nm_init(open_or_create, (string(name) + "_initx").c_str());
	lock_guard<named_mutex> guard(nm_init);
	if (!name || strlen(name) > 127 ||!key ||!value ||!num)
		return NULL;

	{
		// for sm_tools command show i.e. list namespace
		named_mutex namespace_sm(open_or_create, ("sm_namespaces_shared_mutex"));
		lock_guard<named_mutex> guard(namespace_sm);
		shared_memory_object smo(open_or_create, "sm_namespaces", read_write);
		constexpr size_t name_size = 128;
		size_t count = 1024;
		smo.truncate(name_size * count);
		mapped_region rg(smo, read_write);
		char *q = static_cast<char*>(rg.get_address());
		while(*q && --count)
		{
			if (strcmp(q, name) == 0)
				break;
			q +=name_size;
		}
		if (!*q)
			strcpy(q, name);
	} 

	sm_info* info = new sm_info;
	info->name = name;
	info->key_size = key;
	info->value_size = value;
	info->kvi_size = key + value + sizeof size_t;
	info->bucket_size = static_cast<size_t>(num / factor)+1;
	info->max_block = static_cast<size_t>(info->bucket_size + num);

	info->shm = get_shm(name);
	shdmems[name]->truncate(info->kvi_size * info->max_block + 128);
	if (regions.find(name) == regions.end())
		regions[name] = make_shared<mapped_region>(*info->shm, read_write);
	info->region = regions[name];

	size_t* p = static_cast<size_t*>(info->region->get_address());
	*p = version;
	*++p = key;
	*(++p) = value;
	*(++p) = info->bucket_size;
	*(++p) = info->max_block;
	init2(info, p);
	init_mutex(info);
	return static_cast<SM_HANDLE>(info);

}

DLL_API SM_HANDLE sm_client_init(const char* name)
{
	named_mutex nm_init(open_or_create, (string(name) + "_initx").c_str());
	lock_guard<named_mutex> guard(nm_init);
	if (!name)
		return NULL;

	auto shm = get_shm(name);
	if (!total_memory(shm))
		return NULL;

	if (regions.find(name) == regions.end())
		regions[name] = make_shared<mapped_region>(*shm, read_write);
	auto region = regions[name];
	size_t* p = static_cast<size_t*>(region->get_address());

	if (!p || *p!=version)
		return NULL;
	sm_info* info = new sm_info;
	info->name = name;
	info->key_size = *++p;
	info->value_size = *++p;
	info->kvi_size = info->key_size + info->value_size + sizeof size_t;
	info->bucket_size = *++p;
	info->max_block = *++p;
	init2(info, p);

	init_mutex(info);

	info->shm = shm;
	info->region = region;
	return static_cast<SM_HANDLE>(info);
}

DLL_API int sm_set(const SM_HANDLE handle, const char* key, const char* value)
{
	auto info = static_cast<const sm_info*>(handle);
	if (!handle || !key || !value)
		return -2001;

	if (strlen(key) >= info->key_size || strlen(value) >= info->value_size)
		return -2002;
	//cout << "id " << get_bucket_no(info, key) << endl;
	lock_guard<named_mutex> guard(*get_mutex(info, key));

	//获得该key的hash_mode值的桶链表首元素
	auto ak = make_shared<kvi>(info, get_bucket_no(info, key));

	//遍历桶链表
	while (ak)
	{
		//key相同，更新value
		if (strcmp(key, ak->key) == 0)
		{
			//cout << key<<" strcmp(key, ak->key) == 0" << endl;
			strcpy(ak->value, value);
			return 0;
		}

		//key首字节为0，则该桶为空，插入key-value
		if (!*ak->key)
		{
			//cout <<key<< " !*ak->key"<<endl;
			strcpy(ak->key, key);
			strcpy(ak->value, value);
			lock_guard<named_mutex> lg_elements(*info->mu_elements);
			++*info->elements;
			if (ak->key < info->extend_head)
				++*info->used_bucket;
			return 0;
		}

		//链表搜索完毕，该key仍没找到，则找一个空闲块，链表最后一个元素指向空闲块，拷贝key-value到空闲块
		if (!ak->next)
		{
			//cout << key << " ak->next" << endl;
			lock_guard<named_mutex> mb_guard(*info->mt_used_block);
			if (*info->free_block)
			{
				*ak->index = *info->free_block;
				kvi kn(info, *info->free_block);
				strcpy(kn.key, key);
				strcpy(kn.value, value);
				*info->free_block = *kn.index;
			}
			else if (*info->used_block > info->max_block)
				return -2003;
			else
			{
				*ak->index = *info->used_block;
				kvi kn(info, (*info->used_block)++);
				strcpy(kn.key, key);
				strcpy(kn.value, value);
			}
			lock_guard<named_mutex> lg_elements(*info->mu_elements);
			++*info->elements;
			return 0;
		}
		ak = ak->next;
	}
	return -2999; //should never get here
}

DLL_API int sm_get(const SM_HANDLE handle, const char* key, char* value, size_t& len)
{
	if (!handle || !key || !value)
		return -1001;
	auto info = static_cast<const sm_info*>(handle);
	lock_guard<named_mutex> guard(*get_mutex(info, key));
	auto ak = make_shared<kvi>(info, get_bucket_no(info, key));
	while (ak)
	{
		if (strcmp(key, ak->key) == 0)
		{
			size_t ak_len = strlen(ak->value) + 1;
			if (len < ak_len)
				return -1002;
			len = ak_len;
			strcpy(value, ak->value);
			return 0;
		}
		ak = ak->next;
	}
	return -1003;
}

DLL_API int sm_remove(const SM_HANDLE handle, const char* key)
{
	if (!handle || !key)
		return -1;
	auto info = static_cast<const sm_info*>(handle);

	auto ak = make_shared<kvi>(info, get_bucket_no(info, key));
	auto first = ak;
	lock_guard<named_mutex> guard(*get_mutex(info, key));
	std::shared_ptr<kvi> pre_last;
	//如最后的块不是桶中的块，把最后的块key-value复制到要删除的元素块，最后的块key首字母置为0，最后块的前一块的index置为0
	//把最后的块index置为当前空闲块首块，空闲块首块置为最后的块。
	while (ak)
	{
		if (strcmp(key, ak->key) == 0)
		{
			auto last = ak;
			while (last->next)
			{
				pre_last = last;
				last = last->next;
			}
			if (ak != last)
			{
				strcpy(ak->key, last->key);
				strcpy(ak->value, last->value);
			}
			*last->key = 0;
			//如桶链表中不止一块，则pre_last不为nullptr
			if (pre_last)
			{
				*pre_last->index = 0;
				*last->index = *info->free_block;
				*info->free_block = last->id;
			}
			lock_guard<named_mutex> lg_elements(*info->mu_elements);
			--*info->elements;
			if (!*first->key)
				--*info->used_bucket;
			return 0;
		}
		pre_last = ak;
		ak = ak->next;
	}
	return -3002;
}


DLL_API void sm_release(const SM_HANDLE handle)
{
	if (!handle)
		return;
	delete static_cast<const sm_info*>(handle);
}

static size_t slow_true_size(const sm_info*  info)
{
	auto p = info->bucket_head;
	size_t count = 0;
	for (size_t i = 0; i < *info->used_block; ++i)
		if (*(p + i*info->kvi_size))
			++count;
	return count;
}

DLL_API size_t sm_size(const SM_HANDLE handle)
{
	if (!handle)
		return 0;
	return *static_cast<const sm_info*>(handle)->elements;
}

DLL_API size_t sm_memory_use(const SM_HANDLE handle)
{
	if (!handle)
		return 0;
	auto info = static_cast<const sm_info*>(handle);
	return (*info->used_block - 1)*info->kvi_size;
}

DLL_API size_t sm_total_memory(const SM_HANDLE handle)
{
	if (!handle)
		return 0;
	return total_memory(static_cast<const sm_info*>(handle)->shm);
}

DLL_API double sm_avg_depth(const SM_HANDLE handle)
{
	if (!handle)
		return 0;
	auto info = static_cast<const sm_info*>(handle);
	if (!*info->elements)
		return 0;
	return static_cast<double>(*info->elements) / *info->used_bucket;
}

DLL_API size_t sm_key_len(const SM_HANDLE handle)
{
	if (!handle)
		return 0;
	return static_cast<const sm_info*>(handle)->key_size;
}

DLL_API size_t sm_value_len(const SM_HANDLE handle)
{
	if (!handle)
		return 0;
	return static_cast<const sm_info*>(handle)->value_size;
}

DLL_API size_t sm_version()
{
	return version;
}

DLL_API const char* sm_bucket_head(const SM_HANDLE handle)
{
	if (!handle)
		return 0;
	return static_cast<const sm_info*>(handle)->bucket_head;
}
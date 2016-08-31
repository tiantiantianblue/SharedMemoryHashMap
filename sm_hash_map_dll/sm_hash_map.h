#pragma once
#ifdef MYDLL
#define DLL_API extern "C" _declspec(dllexport) 
#else
#define DLL_API extern "C" _declspec(dllimport) 
#endif

typedef void* SM_HANDLE;

//勿在同一个进程中使用sm_server_init和sm_client_init
//name不能含数字且不能超过128字节，key_size为共享内存中key的最大长度，value_size为共享内存中value的最大长度，
//amount为需要存储的key-value数量 返回值为NULL时初始化失败 factor为加载因子 一般使用默认值0.75， factor变大则内存占用量变小，操作速度变慢。
DLL_API SM_HANDLE sm_server_init(const char* name, size_t key_size, size_t value_size, size_t amount, double factor = 0.75);

//在别的进程调用sm_server_init之前 调用该函数 将返回NULL, 跟sm_server_init的版本不一致返回NULL
//返回值为NULL时初始化失败
DLL_API SM_HANDLE sm_client_init(const char* name);

//插入新的key value或对已有的key更新其value，
//返回值 0：成功， -2001：某个输入为NULL， -2002：key或value的长度非法 -2003：共享内存空间已满
DLL_API int sm_set(const SM_HANDLE, const char* key, const char* value);

//输入时len为value的缓冲区大小， 输出len为strlen(value)+1
//返回值 0：成功， -1001：某个输入为NULL， -1002：key或value的长度非法， -1003：key不存在
DLL_API int sm_get(const SM_HANDLE, const char *key, char* value, size_t& len);

//返回值 0：成功， -3001：某个输入为NULL， -3002：key不存在
DLL_API int sm_remove(const SM_HANDLE, const char* key);

//删除handle对应的内存，不释放对应的共享内存
DLL_API void sm_release(const SM_HANDLE handle);

DLL_API size_t sm_size(const SM_HANDLE);

DLL_API size_t sm_memory_use(const SM_HANDLE);

DLL_API size_t sm_total_memory(const SM_HANDLE);
//桶平均深度
DLL_API double sm_avg_depth(const SM_HANDLE);

DLL_API size_t sm_key_len(const SM_HANDLE handle);

DLL_API size_t sm_value_len(const SM_HANDLE handle);

DLL_API size_t sm_version();

//for sm_tools
DLL_API const char* sm_bucket_head(const SM_HANDLE handle);
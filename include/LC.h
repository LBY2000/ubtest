
#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>
#include "PLR.h"
#include "Models.h"
#include <vector>
#include <algorithm>
#include <random>
#include "generator.h"
#include "uniform_generator.h"
#include "util.h"
//#define LOAD_KEYS 600000
#define LOAD_KEYS 2000000//write
//#define LOAD_KEYS 4000000  //read
#define BACK_KEYS 10000
#define record_count_ 10000000
#define chooser 50
class LC{
    
    public:
       int test;

       ycsbc::Generator<uint64_t> *key_chooser_[chooser]; // transaction key gen
       uint64_t LC_id;
       Models models;
       std::map<int,int> total_times;  //temp_var
       int expected;
	   int new_value;
       uint64_t default_key;

       friend class Models;
       friend class PLR;

    public:
  
uint64_t cache_miss[MAX_APP_THREAD][8];
uint64_t cache_hit[MAX_APP_THREAD][8];
uint64_t invalid_counter[MAX_APP_THREAD][8];
uint64_t lock_fail[MAX_APP_THREAD][8];
uint64_t pattern[MAX_APP_THREAD][8];
uint64_t hierarchy_lock[MAX_APP_THREAD][8];
uint64_t handover_count[MAX_APP_THREAD][8];
uint64_t hot_filter_count[MAX_APP_THREAD][8];


      // uint64_t kKeySpace;
       std::vector<uint64_t> exist_keys;
       std::vector<uint64_t> nonexist_keys;  //for_bulk_loding
       LC();
       ~LC();
       bool search(uint64_t &key,uint64_t &val); //仅仅使用down层模型的读
       void load_data();
       void normal_data();
       //这里写其他数据集的生成接口
       void append_model(double slope, double intercept,
                     const typename std::vector<uint64_t>::const_iterator &keys_begin,
                     const typename std::vector<uint64_t>::const_iterator &vals_begin, 
                     size_t size);



       bool model_search(uint64_t &key,uint64_t &val);  //model级别的search但是只搜索了front
       bool model_insert(uint64_t key,uint64_t val);    //front和backup都搜然后进行插入操作

       bool model_batch_search(uint64_t key,uint64_t &val); //model级别的batch查找，包括查找,是完整的LC的search逻辑
       bool upper_search(uint64_t key,uint64_t &val);

};

inline size_t murmur2 ( const void * key, size_t len, size_t seed=0xc70f6907UL)
{
	// 'm' and 'r' are mixing constants generated offline.
	// They're not really 'magic', they just happen to work well.

	const unsigned int m = 0x5bd1e995;
	const int r = 24;

	// Initialize the hash to a 'random' value

	unsigned int h = seed ^ len;

	// Mix 4 bytes at a time into the hash

	const unsigned char * data = (const unsigned char *)key;

	while(len >= 4)
	{
		unsigned int k = *(unsigned int *)data;

		k *= m;
		k ^= k >> r;
		k *= m;
		
		h *= m;
		h ^= k;

		data += 4;
		len -= 4;
	}
	
	// Handle the last few bytes of the input array

	switch(len)
	{
	case 3: h ^= data[2] << 16;
	case 2: h ^= data[1] << 8;
	case 1: h ^= data[0];
	        h *= m;
	};

	// Do a few final mixes of the hash to ensure the last few
	// bytes are well-incorporated.

	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;

	return h;
}







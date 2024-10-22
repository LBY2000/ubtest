#ifndef __COMMON_H__
#define __COMMON_H__

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <atomic>
#include <bitset>
#include <limits>
#include<numa.h>
 //#define BUCKET_SLOTS 4   //read也许有用
#define BUCKET_SLOTS 8  //write
#define FRONT_NUM 128
#define BACK_NUM 32
#define STASH_NUM 256
#define MAX_APP_THREAD 100
typedef unsigned char myuint8_t;
using Key = uint64_t;
using Value = uint64_t;
#define CAS(_p, _u, _v)  (__atomic_compare_exchange_n (_p, _u, _v, false, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE))
class Entry{
   public:
    uint64_t key;
    uint64_t val;
    Entry(){
        key=0;
        val=0;
    };
};
class LeafBucket{
    friend class LC;
    public:
      int64_t sema = 0;
        bool lock(void){
      int64_t val = sema;
      while(val > -1){
	  if(CAS(&sema, &val, val+1))
	      return true;
	  val = sema;
      }
      return false;
  }
    void unlock(void){
      int64_t val = sema;
      while(!CAS(&sema, &val, val-1)){
	  val = sema;
      }
  }
      std::atomic<int> bucket_lock;
      myuint8_t f_version;
      Entry entry[BUCKET_SLOTS];
      myuint8_t r_version;
      LeafBucket(){
        f_version=0;
        r_version=0;
        bucket_lock.store(0);
      }
      void set_consistent(){
        f_version++;
        r_version=f_version;
      }
      bool check_consistent(){
        bool succ=true;
        succ = succ && (r_version == f_version);
        return succ;
      }

};
class LeafNode{
    public:
       uint64_t stash_loc;
       LeafBucket front_buckets[FRONT_NUM];
       LeafBucket backup_buckets[BACK_NUM];
       Entry stash[STASH_NUM];
       
       LeafNode(){
          for(int i=0;i<FRONT_NUM;++i){
            for(int j=0;j<BUCKET_SLOTS;++j){
              this->front_buckets[i].entry[j].key=0;
              this->front_buckets[i].entry[j].val=0;
            }
          }
          for(int i=0;i<BACK_NUM;++i){
            for(int j=0;j<BUCKET_SLOTS;++j){
              this->backup_buckets[i].entry[j].key=0;
              this->backup_buckets[i].entry[j].val=0;
            }
          }
       }
       
};


#endif
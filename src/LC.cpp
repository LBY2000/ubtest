//此版本包含了简易的LC demo以及tree_test以及Hash_test的源码，还有单向model_serach而不是batch_search的模块


#include "LC.h"
#include <algorithm>

#include <iostream>
#include <queue>
#include <utility>
#include <vector>
#include <ctime>
#include <city.h>
#include "generator.h"
#include "uniform_generator.h"
#include "util.h"
uint64_t kkKeySpace = 1024*1024*1024;

inline Key to_key(uint64_t k){  //city_hash形成随机key编码
  return (CityHash64((char *)&k, sizeof(k)) + 1) % kkKeySpace;
}

int num_less=0,num_greater=0,num_mid=0;
#define STRUCT_OFFSET(type, field)                                             \
  (char *)&((type *)(0))->field - (char *)((type *)(0))

//计算结构体内的成员在结构体中的偏移量

thread_local std::queue<uint16_t> hot_wait_queue_lc;


LC::LC():LC_id(0){     //在这里还需要补充tree_index的构造函数中要做的事情
            for(int i=0;i<chooser;++i){
               // key_chooser_[i] = new ycsbc::UniformGenerator(0, record_count_ - 1);
               //这里还是采用placement_new的方式来申请内存吧
               auto addr=RackMemMalloc(sizeof(ycsbc::Generator<uint64_t>), PerfLevel::L0, 0);
               if(addr==nullptr){
                  std::cout << "==== error in malloc key_chooser ====="<<std::endl;
                  exit(0);
               }
               key_chooser_[i]=new(addr) ycsbc::UniformGenerator(0, record_count_ - 1);
               
            }
            default_key=1;
            this->load_data();
            std::sort(exist_keys.begin(), exist_keys.end());
            exist_keys.erase(std::unique(exist_keys.begin(), exist_keys.end()), exist_keys.end());  //去重
            nonexist_keys.erase(std::unique(nonexist_keys.begin(), nonexist_keys.end()), nonexist_keys.end());
            std::sort(exist_keys.begin(), exist_keys.end());
            this->models.bulkload_train(exist_keys,exist_keys);
            uint64_t keyy=exist_keys[30];
            uint64_t vall=0;
          //  std::cout<<"test key: "<<keyy<<std::endl;
            this->model_batch_search(keyy,vall);

          //  std::cout<<"test val: "<<vall<<std::endl;
            keyy=66666666;
            vall=88888888;
            this->model_insert(keyy,vall);
            uint64_t v_test=0;
          //  std::cout<<"test key: "<<keyy<<std::endl;
            this->model_batch_search(keyy,v_test);
          //  std::cout<<"test val: "<<v_test<<std::endl;
          //  to_key(4);

}



void LC::load_data(){  //在这里切换不同负载，现在只考虑一种生成负载
    normal_data();
    std::cout << "==== LOADing ====="<<std::endl;
    std::cout << "==== initial build ====="<<std::endl;
};

void LC::normal_data(){
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> rand_normal(0, 1);

    exist_keys.reserve(LOAD_KEYS);
    for(size_t i=0; i<LOAD_KEYS; i++) {
        double b=rand_normal(gen);
        if(b<0){
            b*=-1;
        }
        uint64_t a = b*1000000000000;
        if(a<=0) {  //在生成端就把0去掉
            i--;
            continue;
        }
        exist_keys.push_back(a);
    }
    nonexist_keys.reserve(BACK_KEYS);
    for(size_t i=0; i<BACK_KEYS; i++){
        double b=rand_normal(gen);
        if(b<0){
            b*=-1;
        }
        uint64_t a = b*1000000000000;
        if(a<0) {
            i--;
            continue;
        }
        nonexist_keys.push_back(a);
    }
}

void LC::append_model(double slope, double intercept,
                     const typename std::vector<uint64_t>::const_iterator &keys_begin,
                     const typename std::vector<uint64_t>::const_iterator &vals_begin, 
                     size_t size){
       SubModel sub;
       sub.slope=slope;
       sub.intercept=intercept;
       auto k=*(keys_begin+size-1);
       sub.anchor_key=k;
      // LeafNode *leaf=new LeafNode;
       LeafNode *leaf=static_cast<LeafNode *>(RackMemMalloc(sizeof(LeafNode), PerfLevel::L0, 0));
       if(leaf==nullptr){
        std::cout << "==== error in malloc leaf ====="<<std::endl;
        exit(0);
       }
       for(int i=0;i<size;i++){
          auto temp_k=*(keys_begin+i);
          int pre_loc=(int)(slope*(double)(temp_k)+intercept);

          for(int j=0;j<BUCKET_SLOTS;j++){
            if(leaf->front_buckets[pre_loc].entry[j].key==0){
                leaf->front_buckets[pre_loc].entry[j].key=temp_k;
                leaf->front_buckets[pre_loc].entry[j].val=temp_k+3;
                break;
            }}}
       sub.leaf_ptr=leaf;
       models.down.push_back(sub);

   return;
}


bool LC::model_insert(uint64_t key,uint64_t val){
   //  bool is_try=false;
    int expected = 0;
    int new_value = 1;
     int loc_up=-100,loc_up_pre,loc_down,loc_pre;
    for(int i=0;i<models.top.size();i++){
       if(models.top[i].anchor_key>key){
          loc_up= (int)(models.top[i].slope*(double)(key)+models.top[i].intercept);
          if(loc_up<0){
            loc_up=0;
          }
          loc_up+=models.top[i].offset; 
          break;
       } 
    }
    for(int i=loc_up-4;i<=loc_up+4;i++){
        if(i<0){
            continue;
        }
        if(models.up[i].anchor_key>key){
            loc_down=(int)(models.up[i].slope*(double)(key)+models.up[i].intercept);
            loc_up_pre=i;
            if(loc_down<0){
                loc_down=0;
            }
            break;
        }
    }
    for(int i=loc_down-4;i<=loc_down+4;i++){
        if(i<0){
            continue;
        }
        if((models.up[loc_up_pre].down)[i].anchor_key>key){
            
            //后续整个的加锁和解锁过程就在这里完成
            loc_pre=(int)((models.up[loc_up_pre].down)[i].slope*(double)(key)+(models.up[loc_up_pre].down)[i].intercept);
            (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre];
            (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4];
          //  try_again:
           // if(is_try){
            //    return false;
          //  }
            //这里在计算完地址后应该先对远端进行写锁
            //自旋锁
            while(1){
                
              if((models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].bucket_lock.compare_exchange_strong(expected,new_value)){
              //  (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].bucket_lock==1;
                break;
                //这里需要原子变量，后续完善的时候再说吧
              }else{
                continue;
              }
           }

           //接下来发起read_batch;
       //    retry:
         //  if(!(models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].check_consistent()){
              // (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].bucket_lock==0;
        //       goto retry;
        //   }
      
           bool is_full=false;
           for(int p=0;p<BUCKET_SLOTS;p++){
               if((models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].entry[p].key==key){
                  (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].entry[p].val=val;
                  (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].bucket_lock.store(0);
                 // (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].bucket_lock==0;
                  return true;

               }else if((models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].entry[p].key==0){
                  (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].entry[p].key=key;
                  (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].entry[p].val=val;
                 // (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].bucket_lock==0;
                 (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].bucket_lock.store(0);
                  return true;

               }else if((models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].entry[p].key>key){
                  if((models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].entry[BUCKET_SLOTS-1].key!=0){
                    is_full=true;
                    break;
                  }else{
                    int begin_loc;
                    for(int j=BUCKET_SLOTS-1;j>=0;--j){
                        if((models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].entry[j].key!=0){
                            begin_loc=j;
                            break;
                        }
                    }
                    for(int t=begin_loc;t>=p;--t){
                        (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].entry[t+1].key=(models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].entry[t].key;
                        (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].entry[t+1].val=(models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].entry[t].val;
                    }
                    (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].entry[p].key=key;
                    (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].entry[p].val=val;
                    //(models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].bucket_lock==0;
                    (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].bucket_lock.store(0);
                    return true;
                  }

               }else{
                  if(p==BUCKET_SLOTS-1){
                     is_full=true;
                  }else{
                     continue;
                  }
               }
           }
           //(models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].bucket_lock==0;
           (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].bucket_lock.store(0);
           int tag_loc=-1;
            if(is_full){
                
                while(1){
                  if((models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].bucket_lock.compare_exchange_strong(expected,new_value)){
                   // (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].bucket_lock==1;
                    break;
                    //这里需要原子变量，后续完善的时候再说吧
                  }
                }
                for(int p=0;p<BUCKET_SLOTS;p++){
                    if((models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[p].key==key){
                        (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[p].val=val;
                      //  (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].bucket_lock==0;
                        (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].bucket_lock.store(0);
                        return true;

                    }else if((models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[p].key==0){
                        (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[p].key=key;
                        (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[p].val=val;
                       // (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].bucket_lock==0;
                        (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].bucket_lock.store(0);
                        return true;
                    }else if((models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[p].key>key){
                        if((models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[BUCKET_SLOTS-1].key!=0){
                            tag_loc=p;
                          //  (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].bucket_lock==0;
                        //    return false;
                              break;
                        }else{
                            int begin_loc;
                            for(int t=BUCKET_SLOTS-2;t>=0;--t){
                                if((models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[t].key!=0){
                                    begin_loc=t;
                                    break;
                                }
                            }
                            for(int t=begin_loc;t>=p;--t){
                                (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[t+1].key=(models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[t].key;
                                (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[t+1].val=(models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[t].val;
                            }
                            (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[p].key=key;
                            (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[p].val=val;
                          //  (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].bucket_lock==0;
                          (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].bucket_lock.store(0);
                            return true;


                       }
                    }else{
                        if(p==BUCKET_SLOTS-1){
                            is_full=true;
                         //   (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].bucket_lock==0;
                          //  return false;
                          break;
                        }
                    }
                }

            }
            if(tag_loc!=-1){
              for(int p=tag_loc;p+1<BUCKET_SLOTS;++p){
                (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[p+1].key=(models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[p].key;
                (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[p+1].val=(models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[p+1].val;
              }
              (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[tag_loc].key=key;
              (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[tag_loc].val=val;
            }else{
              while(1){
              if((models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].bucket_lock.compare_exchange_strong(expected,new_value)){
              //  (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].bucket_lock==1;
                break;
                //这里需要原子变量，后续完善的时候再说吧
              }else{
                continue;
              }
           }
               for(int p=1;p<BUCKET_SLOTS;++p){
                  (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[p].key=0;
                  (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].entry[p].key=0;
               } 
               (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].bucket_lock.store(0);
               (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].bucket_lock.store(0);
               return false;
            }
         //   (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].bucket_lock==0;
        // (models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].bucket_lock.store(0);
        // (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].bucket_lock.store(0);
        //    if(is_full){
               // sleep(1);
               
           //    ++loc_pre;
            //   if(loc_pre>=128){
            //    loc_pre=0;
           //    }
            //   is_try=true;
            //   goto try_again;
          //  }
           // (models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].bucket_lock==0;
            return false;                
        }
    } 



     return false;

  return true;
}

bool LC::model_batch_search(uint64_t key,uint64_t &val){  
   // key=default_key;
   // ++default_key;
    if(key>models.top[models.top.size()-1].anchor_key){
        key=1;
    }
    int loc_up=-100,loc_up_pre,loc_down,loc_pre;
    for(int i=0;i<models.top.size();i++){
       if(models.top[i].anchor_key>key){
          loc_up= (int)(models.top[i].slope*(double)(key)+models.top[i].intercept);
          if(loc_up<0){
            loc_up=0;
          }
          loc_up+=models.top[i].offset; 
          break;
       } 
    }

    for(int i=loc_up-4;i<=loc_up+4;i++){
        if(i<0){
            continue;
        }
        if(models.up[i].anchor_key>key){
            loc_down=(int)(models.up[i].slope*(double)(key)+models.up[i].intercept);
            loc_up_pre=i;
            if(loc_down<0){
                loc_down=0;
            }
            break;
        }
    }
   // return true;
    for(int i=loc_down-4;i<=loc_down+4;i++){
        if(i<0){
            continue;
        }
        if((models.up[loc_up_pre].down)[i].anchor_key>key){
            loc_pre=(int)((models.up[loc_up_pre].down)[i].slope*(double)(key)+(models.up[loc_up_pre].down)[i].intercept);
         retry:
            if((!(models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].check_consistent())&&(!(models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].check_consistent())){
               return false; 
               goto retry;
           }
            

           bool can_out=false;
           for(int p=0;p<BUCKET_SLOTS;p++){
               if((models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].entry[p].key==key){
                  val=(models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].entry[p].val;
                  can_out=true;
                  return true;
              continue;

               }else if((models.up[loc_up_pre].down)[i].leaf_ptr->front_buckets[loc_pre].entry[p].key==0){
                  return false;
                continue;
               }
           }
           if(can_out){
            return true;
           }
            for(int p=0;p<BUCKET_SLOTS;p++){
                if((models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[p].key==key){
                    val=(models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[p].val;
                   // continue;
                    return true;

                }else if((models.up[loc_up_pre].down)[i].leaf_ptr->backup_buckets[loc_pre/4].entry[p].key==0){
                   // continue;
                    return false;
                }
            }
            break;
        }
    } 
    

    return false;
};



bool LC::search(uint64_t &key,uint64_t &find_val){  //down_level_search
        for(int i=0;i<models.down.size();i++){
            if(models.down[i].anchor_key>key){
        //  std::cout<<"we find anchor key is:"<<models.down[i].anchor_key<<std::endl;
            int loc=(int)(models.down[i].slope*(double)(key)+models.down[i].intercept);
            
            for(int j=0;j<BUCKET_SLOTS;j++){
                if(models.down[i].leaf_ptr->front_buckets[loc].entry[j].key==key){
                    find_val=models.down[i].leaf_ptr->front_buckets[loc].entry[j].val;
                    return true;
                }
            }
            break;
        }
    }
    return false;
}



bool LC::upper_search(uint64_t key,uint64_t &val){
//待写
    int loc_up,loc_up_pre,loc_down,loc_pre;
    for(int i=0;i<models.up.size();i++){
       // std::cout<<models.up[i].anchor_key<<std::endl;
        if(models.up[i].anchor_key>key){
            loc_up=i;
            loc_up_pre=(int)(models.up[i].slope*(double)(key)+models.up[i].intercept);
            break;
        }
    }

    for(int i=loc_up_pre-4;i<=loc_up_pre+4;i++){
        if(i<0){
            continue;
        }
        if((models.up[loc_up].down)[i].anchor_key>key){
            loc_pre=(int)((models.up[loc_up].down)[i].slope*(double)(key)+(models.up[loc_up].down)[i].intercept);
            
            for(int j=0;j<BUCKET_SLOTS;j++){
                if((models.up[loc_up].down)[i].leaf_ptr->front_buckets[loc_pre].entry[j].key==key){
                    val=(models.up[loc_up].down)[i].leaf_ptr->front_buckets[loc_pre].entry[j].val;
                    return true;
                }
            }
            break;
        }
    } 

    return false;
};



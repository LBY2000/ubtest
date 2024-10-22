#include <iostream>
#include "LC.h"
#include <getopt.h>
#include <unistd.h>
#include <city.h>
#include "rack_mem.h"
#include "rack_mem_def.h"
using namespace std;

#define N 4000000
std::vector<uint64_t> exist_keys;
std::vector<uint64_t> non_exist_keys;
#define unlikely(x) __builtin_expect(!!(x), 0)
void bindCore(uint16_t core) {

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        std::cout<<"can't bind core!"<<std::endl;
    }
}


#define CACHELINE_SIZE (1 << 6)
#define UNUSED(var) ((void)var)
double read_ratio = 1;
double insert_ratio = 0;
double update_ratio = 0;
double delete_ratio = 0;
double scan_ratio = 0;
size_t table_size = 1000000;
size_t runtime = 4;  //runtime_tag
size_t fg_n = 1;
size_t bg_n = 1;
volatile bool running = false;
std::atomic<size_t> ready_threads(0);
uint64_t kKeySpace = 1024*1024*1024;
inline Key to_key(uint64_t k){  //city_hash形成随机key编码
  return (CityHash64((char *)&k, sizeof(k)) + 1) % kKeySpace;
}


#define COUT_THIS(this) std::cout << this << std::endl;
#define COUT_VAR(this) std::cout << #this << ": " << this << std::endl;
#define COUT_POS() COUT_THIS("at " << __FILE__ << ":" << __LINE__)
#define COUT_N_EXIT(msg) \
  COUT_THIS(msg);        \
  COUT_POS();            \
  abort();
#define INVARIANT(cond)            \
  if (!(cond)) {                   \
    COUT_THIS(#cond << " failed"); \
    COUT_POS();                    \
    abort();                       \
  }

struct alignas(CACHELINE_SIZE) LCParam {
  LC *my_lc;
  uint64_t throughput;
  uint32_t thread_id;
};
typedef LCParam lc_param_t;
inline void parse_args(int argc, char **argv) {
  struct option long_options[] = {
      {"read", required_argument, 0, 'a'},
      {"insert", required_argument, 0, 'b'},
      {"remove", required_argument, 0, 'c'},
      {"update", required_argument, 0, 'd'},
      {"scan", required_argument, 0, 'e'},
      {"table-size", required_argument, 0, 'f'},
      {"runtime", required_argument, 0, 'g'},
      {"fg", required_argument, 0, 'h'},
      {"bg", required_argument, 0, 'i'},
      };
  std::string ops = "a:b:c:d:e:f:g:h:i:";
  int option_index = 0;

  while (1) {
    int c = getopt_long(argc, argv, ops.c_str(), long_options, &option_index);
    if (c == -1) break;

    switch (c) {
      case 0:
        if (long_options[option_index].flag != 0) break;
        abort();
        break;
      case 'a':
        read_ratio = strtod(optarg, NULL);
        INVARIANT(read_ratio >= 0 && read_ratio <= 1);
        break;
      case 'b':
        insert_ratio = strtod(optarg, NULL);
        INVARIANT(insert_ratio >= 0 && insert_ratio <= 1);
        break;
      case 'c':
        delete_ratio = strtod(optarg, NULL);
        INVARIANT(delete_ratio >= 0 && delete_ratio <= 1);
        break;
      case 'd':
        update_ratio = strtod(optarg, NULL);
        INVARIANT(update_ratio >= 0 && update_ratio <= 1);
        break;
      case 'e':
        scan_ratio = strtod(optarg, NULL);
        INVARIANT(scan_ratio >= 0 && scan_ratio <= 1);
        break;
      case 'f':
        table_size = strtoul(optarg, NULL, 10);
        INVARIANT(table_size > 0);
        break;
      case 'g':
        runtime = strtoul(optarg, NULL, 10);
        INVARIANT(runtime > 0);
        break;
      case 'h':
        fg_n = strtoul(optarg, NULL, 10);
        INVARIANT(fg_n > 0);
        break;
      case 'i':
        bg_n = strtoul(optarg, NULL, 10);
        break;
      default:
        abort();
    }
  }

  COUT_THIS("[micro] Read:Insert:Update:Delete:Scan = "
            << read_ratio << ":" << insert_ratio << ":" << update_ratio << ":"
            << delete_ratio << ":" << scan_ratio)
  double ratio_sum =
      read_ratio + insert_ratio + delete_ratio + scan_ratio + update_ratio;
  INVARIANT(ratio_sum > 0.9999 && ratio_sum < 1.0001);  // avoid precision lost
  COUT_VAR(runtime);
  size_t worker_threads_num=fg_n;
  COUT_VAR(worker_threads_num);
 // COUT_VAR(bg_n);

}
void *run_fg(void *param){
  lc_param_t &thread_param = *(lc_param_t *)param;
  uint32_t thread_id = thread_param.thread_id;
  bindCore(thread_id);
  LC *my_lc = thread_param.my_lc;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> ratio_dis(0, 1);
  size_t exist_key_n_per_thread = exist_keys.size() / fg_n;
  size_t exist_key_start = thread_id * exist_key_n_per_thread;
  size_t exist_key_end = (thread_id + 1) * exist_key_n_per_thread;
  std::vector<uint64_t> op_keys(exist_keys.begin() + exist_key_start,
                                   exist_keys.begin() + exist_key_end);

  if (non_exist_keys.size() > 0) {
    size_t non_exist_key_n_per_thread = non_exist_keys.size() / fg_n;
    size_t non_exist_key_start = thread_id * non_exist_key_n_per_thread,
           non_exist_key_end = (thread_id + 1) * non_exist_key_n_per_thread;
    op_keys.insert(op_keys.end(), non_exist_keys.begin() + non_exist_key_start,
                   non_exist_keys.begin() + non_exist_key_end);
  }
  size_t query_i = 0, insert_i = op_keys.size() / 2, delete_i = 0, update_i = 0;
  COUT_THIS("[micro] Worker" << thread_id << " Ready.");
  //size_t query_i = 0, insert_i = 0, delete_i = 0, update_i = 0;
  // exsiting keys fall within range [delete_i, insert_i)
  ready_threads++;
  volatile bool res = false;
  uint64_t dummy_value = 1234;
  UNUSED(res);

  while (!running)
    ;
  uint64_t i=1;
  while (running) {
    double d = ratio_dis(gen);
    if (d <= read_ratio) {  // get
      my_lc->model_batch_search(my_lc->key_chooser_[thread_id]->Next(),dummy_value);
      ++i;

    } else{  // update
      my_lc->model_insert(my_lc->key_chooser_[thread_id]->Next(),dummy_value);
      ++i;

    } 
    thread_param.throughput++;
  }

  pthread_exit(nullptr);
};

void run_benchmark(LC *my_lc, size_t sec) {
 // sec=10;
  pthread_t threads[fg_n];
  lc_param_t lc_params[fg_n];
  // check if parameters are cacheline aligned
  for (size_t i = 0; i < fg_n; i++) {
    if ((uint64_t)(&(lc_params[i])) % CACHELINE_SIZE != 0) {
      COUT_N_EXIT("wrong parameter address: " << &(lc_params[i]));
    }
  }

  running = false;
  for (size_t worker_i = 0; worker_i < fg_n; worker_i++) {
    lc_params[worker_i].my_lc = my_lc;
    lc_params[worker_i].thread_id = worker_i;
    lc_params[worker_i].throughput = 0;
    int ret = pthread_create(&threads[worker_i], nullptr, run_fg,
                             (void *)&lc_params[worker_i]);
    if (ret) {
      COUT_N_EXIT("Error:" << ret);
    }
  }

  COUT_THIS("[micro] prepare data ...");
  while (ready_threads < fg_n) sleep(1);

  running = true;
  std::vector<size_t> tput_history(fg_n, 0);
  size_t current_sec = 0;
  while (current_sec < sec) {
    sleep(1);
    uint64_t tput = 0;
    for (size_t i = 0; i < fg_n; i++) {
      tput += lc_params[i].throughput - tput_history[i];
      tput_history[i] = lc_params[i].throughput;
    }
    COUT_THIS("[micro] >>> sec " << current_sec << " throughput: " << tput);
    ++current_sec;
  }

  running = false;
  void *status;
  for (size_t i = 0; i < fg_n; i++) {
    int rc = pthread_join(threads[i], &status);
    if (rc) {
      COUT_N_EXIT("Error:unable to join," << rc);
    }
  }

  size_t throughput = 0;
  for (auto &p : lc_params) {
    throughput += p.throughput;
  }
  COUT_THIS("[micro] Throughput(op/s): " << throughput / sec);
};






int main(int argc,char **argv){
    /*
    cout<<"Hello?"<<endl;
    LC *my_lc=new LC;
    uint64_t key=666666;
    uint64_t val=888888;
    uint64_t ans=777;
    my_lc->model_insert(key,val);
    my_lc->model_batch_search(key,ans);
    cout<<"the val we test in search is:"<<ans<<endl;*/
    parse_args(argc, argv);
    LC *my_lc=static_cast<LC *>(RackMemMalloc(sizeof(LC), PerfLevel::L0, 0));
    if(my_lc==nullptr){
        std::cout << "==== error in malloc my_lc ====="<<std::endl;
        exit(0);
    }
   // LC *my_lc=new LC;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int64_t> rand_int64(
        0, std::numeric_limits<int64_t>::max());
    exist_keys.reserve(N);
    non_exist_keys.reserve(N);
    for(int i=0;i<N;++i){
        exist_keys.push_back(to_key(i));
        non_exist_keys.push_back(to_key(N+i));
    }
    run_benchmark(my_lc, runtime);






    return 0;
}
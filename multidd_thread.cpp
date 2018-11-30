#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <thread>
#include <list>
#include <optional>
#include <map>
#include <set>
#include <chrono>
#include <thread>
#include <atomic>
#include <functional>

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;
const size_t default_blocksize = 1<<17;

#include "util.cpp"

struct Worker {
  int input, output;
  const long blocksize;
  string ofname;
  Worker(int input, int output, int blocksize, string ofname)
    : input(input), output(output), blocksize(blocksize), ofname(ofname),
      progress(0), state(s_working)
  {}
  
  enum State {
    s_working = 0, s_done, s_fail
  };
  atomic<size_t> progress;
  atomic<State> state;
  chrono::steady_clock::time_point done_at;
  thread thr;
  
  void do_copy() {
    vector<char> buffer(blocksize);
    long u = 0;
    long last_u = 0;
    const long measure_step = 32;
    ssize_t amt;
    while (amt = pread(input, buffer.data(), blocksize, u*blocksize)) {
      if(u > last_u+measure_step){
        posix_fadvise(output, 0, 0, POSIX_FADV_DONTNEED);
        fdatasync(output);
        
        progress = u*blocksize;
        last_u = u;
      }
      if (!write(output, buffer.data(), amt)) {
        clog << "\33[2K\r";
        clog << "Failed write to " << ofname << ": " << strerror(errno) << endl << flush;
        state = s_fail;
        return;
      }
      u++;
    }
    state = s_done;
    done_at = chrono::steady_clock::now();
  }
};


int main(int argc, char* argv[]){
  pair<set<string>, map<string, vector<string> > > parameters = parse_args(argc, argv);
  
  clog << fixed;
  clog.precision(1);

  if(argc == 1){
    print_usage();
  }else if(argc == 2){
    print_usage();
  }else if(argc == 3){

    string infile(argv[1]);
    int i = infile.find("if=");
    if(i != 0 || infile.size() < 4){
      print_usage();
      return 1;
    }
    infile = infile.substr(3, infile.size()-1);

    string outfile(argv[2]);
    vector<string> outfiles;
    i = outfile.find("of=");
    if(i != 0 || outfile.size() < 4){
      print_usage();
      return 1;
    }else{
      for(int start = 3; start < outfile.size(); start++){
        for(int length = 0; length + start < outfile.size() + 1; length++){
          if(outfile[start+length] == ';'){
            if(length != 0){
              outfiles.push_back(outfile.substr(start, length));
              start += length;
            }
            break;
          }else if(length + start == outfile.size()){
            if(length != 0){
              outfiles.push_back(outfile.substr(start, length));
              start += length;
            }
            break;
          }
        }
      }
    }
    if(outfiles.size() == 0){
      cerr << "No files to write to." << endl;
      return 1;
    }
    
    int input = open(infile.c_str(), O_RDONLY);
    if (input == -1) {
      cerr << "Failed to open " << infile << " for reading." << endl;
      cerr << strerror(errno) << endl;
      return 1;
    }
    size_t size;
    {
      struct stat st;
      fstat(input, &st);
      size = st.st_size;
    }
    clog << "Reading from " << infile << " (" << size/MiB << " MiB) and writing to:" << endl;
    for(string s : outfiles) clog << s << endl;
    clog << endl;
    
    using namespace std::chrono;
    using fseconds = duration<double>;
    
    auto start_at = steady_clock::now();
    list<Worker> workers;
    for (string outfile : outfiles) {
      int fd = open(outfile.c_str(), O_WRONLY);
      if (fd == -1) {
        cerr << "Failed to open " << outfile << " for writing." << endl;
        cerr << strerror(errno) << endl;
        exit(1);
      }
      workers.emplace_back(input, fd, default_blocksize, outfile);
      auto &wrk = workers.back();
      wrk.thr = thread([&wrk] () { wrk.do_copy(); });
    }
      
    bool all_done = false;
    while (!all_done) {
      clog << "\33[2K\r";
      all_done = true;
      for (auto &worker : workers) {
        switch (worker.state) {
          case Worker::s_working:
            clog << 100.*worker.progress/size << " %\t";
            all_done = false;
          break;
          case Worker::s_fail:
            clog << "fail\t";
          break;
          case Worker::s_done:
            clog << "done\t";
          break;
        }
      }
      clog << flush;
      if (all_done) break;
      this_thread::sleep_for(milliseconds(200));
    }
    
    // Print writing speeds
    clog << "\33[2K\r";
    for (auto &worker : workers) {
      if (worker.state == Worker::s_done) {
        double elapsed = duration_cast<fseconds>(worker.done_at - start_at).count();
        clog << size/MiB/elapsed << "\t";
      } else {
        clog << "(fail)\t";
      }
    }
    clog << "MiB/s" << endl << flush;
    
    for (auto &worker : workers) {
      worker.thr.join();
      close(worker.output);
    }
    close(input);
  }else{
    print_usage();
    return 1;
  }
  
}

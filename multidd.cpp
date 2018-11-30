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

#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>

using namespace std;
const size_t default_blocksize = 1<<17;

#include "util.cpp"

void do_copy(istream &input, vector<int> outputs, const long blocksize){
    using namespace std::chrono;
    using fseconds = duration<double>;
    void *buffer = aligned_alloc(1<<12, blocksize);
    cout << buffer << endl;
    long u = 0;
    long last_u = 0;
    const long measure_step = 2;
    auto last_time = steady_clock::now();
    cerr << fixed;
    while (input) {
      input.read((char*)buffer, blocksize);
      if(u > last_u+measure_step){
        // for (int output : outputs) {
        //   posix_fadvise(output, 0, 0, POSIX_FADV_DONTNEED);
        // }
        // for (int output : outputs) {
        //   fdatasync(output);
        // }
        
        double amount = u*blocksize / MiB;
        auto cur_time = steady_clock::now();
        auto time = cur_time - last_time;
        last_time = cur_time;
        double speed = blocksize*measure_step / MiB / duration_cast<fseconds>(time).count();
        cerr << "\33[2K\r" << amount << " MiB; " << speed << " MiB/s" << flush;
        last_u = u;
      }
      for(int output : outputs){
        // cout << input.gcount() << endl;
        // cout << "fwrite(" << buffer << ", 1, " << blocksize << ", " << output << ");" << endl;
        if (!write(output, buffer, blocksize)) {
          cerr << "Failed write: " << strerror(errno) << endl;
          exit(1);
        }
      }
      u++;
    }
    
    free(buffer);
    cout << endl;
}



int main(int argc, char* argv[]){

  pair<set<string>, map<string, vector<string> > > parameters = parse_args(argc, argv);

  cout.precision(1);  

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
    }else{
      clog << "Reading from " << infile << " and writing to:" << endl;
      for(string s : outfiles) clog << s << endl;
      clog << endl;
    }
    ifstream input(infile, ifstream::binary);
    if(!input){
      cerr << "Failed to open " << infile << " in binary read mode." << endl;
      cerr << strerror(errno) << endl;
    }
    vector<int> outputs;
    for(string outfile : outfiles){
      int fd = open(outfile.c_str(), O_WRONLY | O_DIRECT);
      if (fd == -1) {
        cerr << "Failed to open" << outfile << " in binary write mode." << endl;
        cerr << strerror(errno) << endl;
        return 1;
      }
      outputs.push_back(fd);
    }

    do_copy(input, outputs, default_blocksize);
    for (int output : outputs) {
      close(output);
    }
  }else{
    print_usage();
    return 1;
  }
  
}

static const double MiB = 1024*1024;

void print_usage(){
  cerr << endl;
  cerr << "Usage:" << endl << "multidd if=file.iso [of=drive1;drive2;drive3 | --write-all-usb]" << endl;
}

vector<string> split(string input, char delimiter){
  vector<string> result;
  string temporary = input;
  int i = temporary.find(delimiter);
  while(i != -1){
    if(i == 0){
      temporary = temporary.substr(1, temporary.size());
    }else{
      result.push_back(temporary.substr(0, i));
      temporary = temporary.substr(i+1, temporary.size());
    }
    i = temporary.find(delimiter);
  }
  if(temporary.size() > 0) result.push_back(temporary);
  return result;
}

optional<pair<string, vector<string>>> extract_parameter(string s){
  int i = s.find('=');
  if(i == -1) return {};
  else return {{s.substr(0, i), split(s.substr(i+1, s.size()), ';')}};
}

pair<set<string>, map<string, vector<string>>> parse_args(int argc, char* argv[]){
  map<string, vector<string>> variables;
  set<string> flags;
  for(int i = 1; i < argc; i++){
    string param(argv[i]);
    optional<pair<string, vector<string>>> variable = extract_parameter(param);
    if(variable.has_value()) variables[variable.value().first] = variable.value().second;
    else flags.insert(param);
  }
  return {flags, variables};
}


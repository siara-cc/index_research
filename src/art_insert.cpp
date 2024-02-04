#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "art.cpp"

using namespace std;

double time_taken_in_secs(clock_t t) {
  t = clock() - t;
  return ((double)t)/CLOCKS_PER_SEC;
}

clock_t print_time_taken(clock_t t, const char *msg) {
  double time_taken = time_taken_in_secs(t); // in seconds
  std::cout << msg << time_taken << std::endl;
  return clock();
}

int main(int argc, char *argv[]) {

  art_tree at;
  art_tree_init(&at);
  std::vector<std::string> lines;

  clock_t t = clock();
  fstream infile;
  infile.open(argv[1], ios::in);
  int line_count = 0;
  if (infile.is_open()) {
    string line;
    string prev_line = "";
    while (getline(infile, line)) {
      if (line == prev_line)
         continue;
      // sb.append((const uint8_t *) line.c_str(), line.length(), (const uint8_t *) line.c_str(), line.length() > 6 ? 7 : line.length());
      // sb.append((const uint8_t *) line.c_str(), line.length());
      //sb.insert((const uint8_t *) line.c_str(), line.length(), (const uint8_t *) line.c_str(), line.length() > 6 ? 7 : line.length());
      //sb.insert((const uint8_t *) line.c_str(), line.length());
      lines.push_back(line);
      art_insert(&at, (const uint8_t *) line.c_str(), line.length(), (void *) lines[line_count].c_str());
      prev_line = line;
      line_count++;
      if ((line_count % 100000) == 0) {
        cout << ".";
        cout.flush();
      }
    }
    infile.close();
  }
  std::cout << std::endl;
  t = print_time_taken(t, "Time taken for insert/append: ");

  int err_count = 0;
  line_count = 0;
  bool success = false;
  for (int i = 0; i < lines.size(); i++) {
    std::string& line = lines[i];
    uint8_t *res_val = (uint8_t *) art_search(&at, (const uint8_t *) line.c_str(), line.length());
    if (res_val != NULL) {
      int val_len = line.length() > 6 ? 7 : line.length();
      if (memcmp(line.c_str(), (const void *) res_val, val_len) != 0) {
        printf("key: [%.*s], val: [%.*s]\n", (int) line.length(), line.c_str(), val_len, res_val);
        err_count++;
      }
      //free(res_val);
    } else {
      std::cout << line << std::endl;
      err_count++;
    }
    line_count++;
  }
  t = print_time_taken(t, "Time taken for retrieve: ");
  printf("Lines: %d, Errors: %d\n", line_count, err_count);

}

#include <iostream>
#include <fstream>
#include <set>
#include <map>
#include <sstream>
#include <algorithm>
#include <functional>

typedef unsigned DocID;
typedef unsigned WordID;

const int max_docs = 100;

const char* corpus_in_filename = "./corpus-en.txt";
const char* words_in_filename = "./words.txt";

const char* header_out_filename = "./word_to_docids.h";
const char* index_out_filename = "./out_index.txt"; // word_id doc_id1 doc_id2 doc_id3 ...
const char* freqs_out_filename = "./out_freqs.txt"; // doc_cnt freq word_id word_str

int main() {
  std::map<std::string, unsigned long long> word_freqs;
  std::map<std::string, std::set<DocID> > word_to_doc_ids;

  std::ifstream words_in_file(words_in_filename);
  if (!words_in_file) {
    std::cerr << "Error: could not read file: " <<  words_in_filename << std::endl;
    return EXIT_FAILURE;
  }
  std::string word;
  while (words_in_file >> word) {
    if (word.find("\n") != std::string::npos) std::cerr << "Bad word: '" << word << "'.\n";
    word_freqs[word] = 0;
    word_to_doc_ids[word] = std::set<DocID>();
  }
  std::cerr << word_freqs.size() << " words loaded into the index." << std::endl;

  std::ifstream corpus_in_file(corpus_in_filename);
  if (!corpus_in_file) {
    std::cerr << "Error: could not read file: " <<  corpus_in_filename << std::endl;
    return EXIT_FAILURE;
  }

  DocID doc_id = 1;
  while (corpus_in_file.good()) {
    if (doc_id % 10000 == 0)
      std::cerr << "Processed " << doc_id << " documents (" << word_freqs.size() << " words in index).\n";
    std::string line, word;
    std::getline(corpus_in_file, line);
    std::istringstream line_words(line);

    while (line_words >> word){
      if (word_freqs.find(word) == word_freqs.end())
        continue; // ignore words that we aren't indexing
      word_freqs[word]++;
      word_to_doc_ids[word].insert(doc_id);
    }

    if (doc_id == max_docs) break;
    doc_id++;
  }
  corpus_in_file.close();

  // Sort the words in reverse order of frequency
  typedef std::function<bool(std::pair<std::string, unsigned>, std::pair<std::string, unsigned>)> Comparator;
  Comparator compFunctor = [](std::pair<std::string, unsigned> a, std::pair<std::string, unsigned> b) {
    return a.second == b.second ? a.first > b.first : a.second > b.second; // reverse order
  };
  std::set<std::pair<std::string, unsigned>, Comparator> sorted_words(word_freqs.begin(), word_freqs.end(), compFunctor);

  std::ofstream index_out_file(index_out_filename);
  std::ofstream freqs_out_file(freqs_out_filename);
  std::ofstream header_out_file(header_out_filename);

  header_out_file << "uint32_t* word_to_docids[" << word_freqs.size() << "];\n";
  header_out_file << "uint32_t word_to_docids_bin[] = {" << word_freqs.size() << ",";
  unsigned index_size = 1;

  WordID word_id = 1;
  for (auto const& x : sorted_words) {
    freqs_out_file << word_to_doc_ids[x.first].size() << " " << x.second << " " << word_id << " " << x.first << std::endl;
    index_out_file << word_id << " ";
    header_out_file << "\n" << word_to_doc_ids[x.first].size() << ",";
    index_size += 1 + word_to_doc_ids[x.first].size();
    for (auto const& doc_id : word_to_doc_ids[x.first]) {
      index_out_file << doc_id << " ";
      header_out_file << doc_id << ",";
    }
    index_out_file << std::endl;
    word_id++;
  }

  std::cerr << "Index size (word_to_docids_bin): " << index_size << " * sizeof(uint32_t) = " << index_size*sizeof(uint32_t) << std::endl;

  header_out_file << "};\n";

  index_out_file.close();
  freqs_out_file.close();
  header_out_file.close();

  return EXIT_SUCCESS;
}

#include <iostream>
#include <string>
#include <map>
#include <functional>
#include <memory>
#include <fstream>
#include <vector>
#include <iterator>
#include <algorithm>

#include <myhtml/myhtml.h>
#include <myhtml/serialization.h>
#include <mycss/selectors/serialization.h>
#include <modest/finder/finder.h>

using namespace std;

static const char* helptext = R"(hq (html query) - commandline HTML processor © Robin Broda, 2018
Usage: %s [options] <selector> <mode> [mode argument]

Options:
  -h, --help
    show this text
  -f, --file <file>
    file to read (defaults to stdin)
  -d, --delimiter <delim>
    delimiter character to use between results (defaults to newline)
  -0, --null
    uses \0 as delimiter

  <selector>
    CSS selector to match against
  <mode>
    processing mode
    may be one of { data, text, attr }:
      data - return raw html of matching elements
      text - return inner text of matching elements
      attr - return attribute value of matching elements
        <mode argument: attr>
          attribute to return

Examples:
  curl -sSL https://example.com | %s a data
  curl -sSL https://example.com | %s a attr href
)";

static map<const string, bool> flags = {
  {"dirtyargs", false}
};

static map<const string, string> state = { // global state
  {"progname", "hq"}, // program name
  {"file", "-"}, // input file path, or - for stdin
  {"delim", "\n"}, // result delimiter
  {"selector", ""}, // matching selector
  {"mode", ""}, // output mode
  {"data", ""}, // read input data
  {"modearg", ""} // mode argument (optional)
};

bool readarg(int &argc, const char** &argv, string argname, const bool die_on_err = true){
  if(argc > 1){
    state[argname] = argv[1];
    argv++;
    argc--;
    flags["dirtyargs"] = true;
    return true;
  }else{
    if(die_on_err){
      cerr << "no " << argname << " given" << endl;
      exit(EXIT_FAILURE);
    }
    return false;
  }
}

bool readfile(string filename, string &target){
  shared_ptr<istream> input;
  if(filename == "-"){
    input.reset(&cin, [](...){});
  }else{
    input.reset(new ifstream(filename.c_str()));
    if(!input.get()->good()) return false;
  }
  target = string(istreambuf_iterator<char>(*input.get()), {});
  return true;
}

template <typename T> inline bool vec_has(vector<T> &vec, T val){
  return std::find(vec.begin(), vec.end(), val) != vec.end();
}

static map<const char, const string> option_longopts = { // maps shortopts to longopts from option_handlers
  {'h', "help"},
  {'f', "file"},
  {'d', "delimiter"},
  {'0', "zero"}
};

static map<const string, const function<void(int&, const char**&)>> option_handlers = { // maps longopts to functions
  {"help", [](int &argc, const char** &argv) {
    fprintf(stderr, helptext, state["progname"].c_str(), state["progname"].c_str(), state["progname"].c_str());
    exit(EXIT_FAILURE);
  }},
  {"file", [](int &argc, const char** &argv) {
    readarg(argc, ++argv, "file");
    argv--;
  }},
  {"delimiter", [](int &argc, const char** &argv) {
    readarg(argc, ++argv, "delim");
    argv--;
  }},
  {"zero", [](int &argc, const char** &argv) {
    state["delim"] = "\0";
  }}
};

static map<const string, const function<void(myhtml_tree_node_t*)>> mode_handlers = { // maps modes to functions
  {"data", [](myhtml_tree_node_t* node) {
    myhtml_serialization_tree_callback(node, [](const char* data, size_t len, void* ctx) -> unsigned int {
      printf("%.*s", static_cast<int>(len), data);
      return 0;
    }, nullptr);
    printf("%c", state["delim"][0]);
  }},

  {"text", [](myhtml_tree_node_t* node) {
    string rendered = "";

    static vector<char> collapsible = {' ', '\t', '\n', '\r'};
    static vector<unsigned long> breaking = {
      MyHTML_TAG_BR,
      MyHTML_TAG_P
    };

    myhtml_tree_node_t* node_iter = node->child;
    while(node_iter){
      const char* text_c = myhtml_node_text(node_iter, nullptr);
      string text = "";
      if(text_c != nullptr) text += text_c;

      if(node_iter->tag_id == MyHTML_TAG__TEXT){
        // collapse whitespace to single character
        string::iterator nend = unique(text.begin(), text.end(), [](char c1, char c2) -> bool {
          return vec_has(collapsible, c1) && vec_has(collapsible, c2);
        });
        text.resize(static_cast<unsigned long>(nend-text.begin()));

        // replace whitespace with space
        replace_if(text.begin(), text.end(), [](char c) -> bool {
          return vec_has(collapsible, c);
        }, ' ');

        rendered += text;
      }

      if(node_iter->child) node_iter = node_iter->child;
      else{
        while(node_iter != node && node_iter->next == nullptr) node_iter = node_iter->parent;
        if(node_iter == node) break;

        if(vec_has(breaking, node_iter->tag_id)){ // <br/>
          rendered += "\n";
        }

        node_iter = node_iter->next;
      }
    }

    size_t index = 0;
    while((index = rendered.find("\n ", index)) != string::npos){ // clear whitespace before multiline content
      rendered.erase(index+1, 1);
    }
    index = 0;
    while((index = rendered.find(" \n", index)) != string::npos){ // clear whitespace after multiline content
      rendered.erase(index, 1);
    }

    while(vec_has(collapsible, rendered[0])) rendered.erase(0, 1); // clear whitespace before single-line content
    while(vec_has(collapsible, *(rendered.end()-1))) rendered.erase(rendered.length()-1, 1); // clear whitespace after single-line content

    cout << rendered;
    printf("%c", state["delim"][0]);
  }},

  {"attr", [](myhtml_tree_node_t* node) {
    if(state["modearg"].length() == 0){
      cerr << "no attr name given" << endl;
      exit(EXIT_FAILURE);
    }

    myhtml_token_node_t* token = node->token;
    if(token == nullptr) return;

    myhtml_token_attr_t* attr = token->attr_first;
    if(attr == nullptr) return;

    do{
      if(state["modearg"] == mycore_string_data(&attr->key)){
        cout << mycore_string_data(&attr->value);
        printf("%c", state["delim"][0]);
      }

      if(attr != token->attr_last) attr = attr->next;
    }while(attr != token->attr_last);
  }}
};

void parseopts(int &argc, const char** &argv){
  while(argc > 1){
    if(argv[1][0] == '-'){ // is opt
      if(argv[1][1] == '-'){ // is long opt
        if(argv[1][2] == '\0'){ // stop parsing opts after '--'
          break;
        }else{
          try{
            option_handlers[argv[1]+2](argc, argv);
          }catch(bad_function_call&){
            cerr << "invalid long option '" << argv[1] << "'" << endl;
            exit(EXIT_FAILURE);
          }
        }
      }else{ // is short opt
        while(*++argv[1] != '\0'){ // iterate through short opts
          try{
            option_handlers[option_longopts[argv[1][0]]](argc, argv);
          }catch(bad_function_call&){
            cerr << "invalid short option '-" << argv[1][0] << "'" << endl;
            exit(EXIT_FAILURE);
          }
          if(flags["dirtyargs"]){
            flags["dirtyargs"] = false;
            break;
          }
        }
      }
      argv++;
      argc--;
    }else{
      break;
    }
  }
}

int main(int argc, const char* argv[]){
  state["progname"] = argv[0];

  parseopts(argc, argv);
  readarg(argc, argv, "selector");
  readarg(argc, argv, "mode");
  readarg(argc, argv, "modearg", false);
  if(!readfile(state["file"], state["data"])){
    cerr << "failed reading '" << state["file"] << "'" << endl;
    exit(EXIT_FAILURE);
  }

  myhtml_t* myhtml = myhtml_create();
  myhtml_tree_t* html_tree = myhtml_tree_create();
  mycss_t* mycss = mycss_create();
  mycss_entry_t* css_entry = mycss_entry_create();

  vector<function<mystatus_t(void)>> modest_setup = { // init functions
    bind(myhtml_init, myhtml, MyHTML_OPTIONS_DEFAULT, 1, 0),
    bind(myhtml_tree_init, html_tree, myhtml),
    bind(myhtml_parse, html_tree, MyENCODING_UTF_8, state["data"].c_str(), state["data"].length()),
    bind(mycss_init, mycss),
    bind(mycss_entry_init, mycss, css_entry)
  };

  for(function<mystatus_t(void)> modest_setup_func : modest_setup){ // init and check for errors
    if(modest_setup_func()){
      cerr << "failed to init (modest_setup)" << endl;
      exit(EXIT_FAILURE);
    }
  }

  modest_finder_t* finder = modest_finder_create_simple();
  mystatus_t mystatus;
  mycss_selectors_list_t* selectors_list = mycss_selectors_parse(
    mycss_entry_selectors(css_entry),
    MyENCODING_UTF_8,
    state["selector"].c_str(), state["selector"].length(),
    &mystatus
  );

  if(selectors_list == nullptr || (selectors_list->flags & MyCSS_SELECTORS_FLAGS_SELECTOR_BAD)){
    cerr << "bad selector '" << state["selector"] << "'" << endl;
    exit(EXIT_FAILURE);
  }

  myhtml_collection_t* collection = nullptr;
  modest_finder_by_selectors_list(finder, html_tree->node_html, selectors_list, &collection);

  if(collection){
    try{
      for(myhtml_tree_node_t* node : vector<myhtml_tree_node_t*>(collection->list, collection->list+collection->length)){
        mode_handlers[state["mode"]](node);
      }
    }catch(bad_function_call&){
      cerr << "invalid mode '" << state["mode"] << "'" << endl;
      exit(EXIT_FAILURE);
    }
  }

  // destroy modest datastructures
  myhtml_collection_destroy(collection);
  mycss_selectors_list_destroy(mycss_entry_selectors(css_entry), selectors_list, true);
  modest_finder_destroy(finder, true);
  mycss_destroy(css_entry->mycss, true);
  mycss_entry_destroy(css_entry, true);
  myhtml_destroy(html_tree->myhtml);
  myhtml_tree_destroy(html_tree);
}

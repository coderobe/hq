#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <myhtml/myhtml.h>
#include <myhtml/serialization.h>
#include <mycss/selectors/serialization.h>
#include <modest/finder/finder.h>

char* readeof(){
  const static int buffer_size = 1024;
  char buffer[buffer_size];
  size_t content_size = 1; // \0
  char* content = malloc(sizeof(char)*buffer_size);
  if(content == NULL){
    perror("Failed to allocate");
    exit(EXIT_FAILURE);
  }
  content[0] = '\0';
  while(fgets(buffer, buffer_size, stdin)){
    char* content_old = content;
    content_size += strlen(buffer);
    content = realloc(content, content_size);
    if(content == NULL){
      perror("Failed to allocate");
      free(content_old);
      exit(EXIT_FAILURE);
    }
    strcat(content, buffer);
  }
  return content;
}

unsigned int serializer_log(const char* data, size_t len, void* ctx){
  printf("%.*s", (int)len, data);
  return 0;
}

void opthandler(const char* arg, const char* progname){
  if(!strcmp(arg, "help") || !strcmp(arg, "h")){
    fprintf(stderr, "hq (html query) - commandline HTML processor © Robin Broda, 2018\n");
    fprintf(stderr, "Usage: %s [options] <selector> <mode> [mode argument]\n\n", progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "-h, --help\tshow this text\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "<selector>\tselector to match\n");
    fprintf(stderr, "<mode>\t\tprocessing mode\n");
    fprintf(stderr, "\t\tmay be one of { data, text, attr }:\n");
    fprintf(stderr, "\t\tdata - return raw html of matching elements\n");
    fprintf(stderr, "\t\ttext - return inner text of matching elements\n");
    fprintf(stderr, "\t\tattr - return attribute value X of matching elements\n");
    fprintf(stderr, "\t\t\t[mode argument] - attribute to return\n");
    exit(EXIT_SUCCESS);
  }
}

int main(int argc, const char* argv[]){
  if(argc == 1) opthandler("help", argv[0]);

  size_t shifts = 0; // offset of new argv
  while(argc > 1){
    if(argv[1][0] == '-'){
      const char* arg = argv[1];
      if(arg[1] == '-'){
        const char* longarg = arg+2;
        opthandler(longarg, 0[argv-shifts]);
      }else{
        for(size_t i = 1; i < strlen(arg); i++){
          const char shortarg[2] = { arg[i], '\0' };
          opthandler(shortarg, 0[argv-shifts]);
        }
      }
      shifts++;
      argv++;
      argc--;
    }else{
      argv[0] = 0[argv-shifts]; // restore argv[0]
      break;
    }
  }

  const char* selector;
  if(argc > 1){
    selector = argv[1];
  }else{
    fprintf(stderr, "No selector given\n");
    exit(EXIT_FAILURE);
  }

  const char* mode;
  if(argc > 2){
    mode = argv[2];
  }else{
    fprintf(stderr, "No mode given\n");
    exit(EXIT_FAILURE);
  }
  
  char* input = readeof();

  myhtml_t* myhtml = myhtml_create();
  mystatus_t mystatus = myhtml_init(myhtml, MyHTML_OPTIONS_DEFAULT, 1, 0);
  if(mystatus){
    fprintf(stderr, "Failed to init MyHTML\n");
    exit(EXIT_FAILURE);
  }

  myhtml_tree_t* html_tree = myhtml_tree_create();
  mystatus = myhtml_tree_init(html_tree, myhtml);
  if(mystatus){
    fprintf(stderr, "Failed to init MyHTML tree\n");
    exit(EXIT_FAILURE);
  }

  mystatus = myhtml_parse(html_tree, MyENCODING_UTF_8, input, strlen(input));
  if(mystatus){
    fprintf(stderr, "Failed to parse HTML\n");
    exit(EXIT_FAILURE);
  }

  mycss_t* mycss = mycss_create();
  mystatus = mycss_init(mycss);
  if(mystatus){
    fprintf(stderr, "Failed to init MyCSS\n");
    exit(EXIT_FAILURE);
  }

  mycss_entry_t* css_entry = mycss_entry_create();
  mystatus = mycss_entry_init(mycss, css_entry);
  if(mystatus){
    fprintf(stderr, "Failed to init MyCSS entry\n");
    exit(EXIT_FAILURE);
  }

  modest_finder_t* finder = modest_finder_create_simple();

  mycss_selectors_list_t* selectors_list = mycss_selectors_parse(
    mycss_entry_selectors(css_entry),
    MyENCODING_UTF_8,
    selector, strlen(selector), &mystatus
  );

  if(selectors_list == NULL || (selectors_list->flags & MyCSS_SELECTORS_FLAGS_SELECTOR_BAD)){
    fprintf(stderr, "Bad selector\n");
    exit(EXIT_FAILURE);
  }

  myhtml_collection_t* collection = NULL;
  modest_finder_by_selectors_list(finder, html_tree->node_html, selectors_list, &collection);

  if(collection){
    for(size_t i = 0; i < collection->length; i++){
      if(!strcmp(mode, "text")){
        myhtml_serialization_tree_callback(collection->list[i]->child, serializer_log, NULL);
        printf("\n");
      }else if(!strcmp(mode, "data")){
        myhtml_serialization_tree_callback(collection->list[i], serializer_log, NULL);
        printf("\n");
      }else if(!strcmp(mode, "attr")){
        const char* attr_name;
        if(argc > 3){
          attr_name = argv[3];
        }else{
          fprintf(stderr, "No attr name given");
          exit(EXIT_FAILURE);
        }
        myhtml_tree_node_t* node = collection->list[i];
        myhtml_token_node_t* token = node->token;
        if(token == NULL) continue;
        myhtml_token_attr_t* attr = token->attr_first;
        if(attr == NULL) continue;

        do{
          if(!strcmp(attr_name, mycore_string_data(&attr->key))){
            printf("%s\n", mycore_string_data(&attr->value));
          }
          if(attr != token->attr_last) attr = attr->next;
        }while(attr != token->attr_last);
      }else{
        fprintf(stderr, "invalid mode: '%s'\n", mode);
        exit(EXIT_FAILURE);
      }
    }
  }

  // cleanup
  myhtml_collection_destroy(collection);
  mycss_selectors_list_destroy(mycss_entry_selectors(css_entry), selectors_list, true);
  modest_finder_destroy(finder, true);
  mycss_destroy(css_entry->mycss, true);
  mycss_entry_destroy(css_entry, true);
  myhtml_destroy(html_tree->myhtml);
  myhtml_tree_destroy(html_tree);
  free(input);
  return 0;
}
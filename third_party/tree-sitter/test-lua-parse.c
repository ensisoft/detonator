
// build with
// gcc -o test-lua test-lua-parse.c lua/parser.c lua/scanner.c src/lib.c -I include/ -O3

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <tree_sitter/api.h>

// Declare the `tree_sitter_json` function, which is
// implemented by the `tree-sitter-json` library.
//TSLanguage *tree_sitter_json();
TSLanguage* tree_sitter_lua();

int main() {
  // Create a parser.
  TSParser *parser = ts_parser_new();

  // Set the parser's language (JSON in this case).
  ts_parser_set_language(parser, tree_sitter_lua());

  // Build a syntax tree based on source code stored in a string.
  //const char *source_code = "[1, null]";
  const char* source_code =
  "local meh = obj.keke\n";

  TSTree *tree = ts_parser_parse_string(
    parser,
    NULL,
    source_code,
    strlen(source_code)
  );

  // Get the root node of the syntax tree.
  TSNode root_node = ts_tree_root_node(tree);

//TSNode call = ts_node_child_by_field_name(root_node, "call", strlen("call"));

  // Print the syntax tree as an S-expression.
  char *string = ts_node_string(root_node);
  printf("%s\n", string);

  // Free all of the heap-allocated memory.
  free(string);
  ts_tree_delete(tree);
  ts_parser_delete(parser);
  return 0;
}

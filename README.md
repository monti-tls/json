Yet another single-header C++ JSON library. Just under 2000 SLOC.
Supports standard JSON, extended with comments and include directive.

It has two API levels : a standard AST tree that can be obtained
by parsing some input, that can then be explored (and re-serialized
into JSON), and a second level using the 'template' mechanism.

json::Template allows to describe a data structure using bindings
to `lvalues` or `rvalues`. A template can then be synthetized into
JSON, or extracted from JSON (thus writing parsed values to the `lvalues`).

IMHO, this is what lacks from most of the JSON frameworks I encoutered.
I want to be able to express the fact that I expect some structure from
the JSON that I parse, by another method than the usual checking of node
types. With this template mechanism, I can just do :

```
int a;
std::string b;
json:::Template tpl;
tpl.bind("a", a);
tpl.bind("b", b);
tpl.extract(std::cin);
```

All the checking will be done automatically.
This header supports most native types (all PODs), std::vector<>, std::map<>,
raw data as hex strings and custom types. See the `main.cpp` file for some examples.

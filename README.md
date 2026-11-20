# Miniexec

Miniexec is a miniature implementation of std::execution for execution control of structured concurrent programs. The implementation is a good faithed attempt to interpret what is defined by [cppreference.com](https://cppreference.com/cpp/execution) and [c++draft](https://eel.is/c++draft/exec). 

## Caveats

This section details some of the personal liberties that have been taken in implementing the library that differ for the exact specifications:

- CPOs: I did not feel that the traditional `tag_invoke` would be needed. Rather than overloading from your own code, simply vendor miniexec and then apply the overloading that you would need directly into your own copy of the library.



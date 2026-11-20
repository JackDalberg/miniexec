# A Mini implementation of std::execution

This library is a miniature implementation of std::execution for execution control of structured concurrent programs. The implementation is a mostly good faithed attempt to interpret what is defined by [cppreference.com](https://cppreference.com/cpp/execution) and [c++draft](https://eel.is/c++draft/exec). The caveats are that I do not implement CPOs allowing for tagged_invoke semantics for ease of implementation and I only implement the pieces that I use in my own programs.

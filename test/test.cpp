#include "url.hpp"

#include <cassert>

int main()
{
    // Check if this repository is online
    assert(url::get("github.com/petter-holmberg/url"));
}

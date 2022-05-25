#include "json.h"

#include <sstream>

int main(int argc, char** argv)
{
    std::cout << "** Example 1 - simple serialize" << std::endl;
    {
        json::Template tpl;
        tpl.bind("a", 123);
        tpl.bind("b", std::vector<int>{46, 89});

        // Serialize template
        json::synthetize(tpl, std::cout);
        // {
        //     "a": 123,
        //     "b": [46, 89]
        // }
    }

    std::cout << std::endl << std::endl;
    std::cout << "** Example 2 - simple extract" << std::endl;
    {
        int a;
        std::vector<int> b;

        json::Template tpl;
        tpl.bind("a", a);
        tpl.bind("b", b);

        std::string str = "{\"a\": 456, \"b\": [33, 578]}";
        std::istringstream iss(str);

        // Extract values to bound variables
        json::extract(tpl, iss);

        std::cout << "a = " << a << std::endl;
    }

    std::cout << std::endl;
    std::cout << "** Example 3 - Complex types and nesting" << std::endl;
    {
        std::map<std::string, std::vector<int>> a;

        json::Template tpl;
        tpl.bind("a", a);

        int c;
        std::string d;
        json::Template obj_tpl;
        obj_tpl.bind("c", c);
        obj_tpl.bind("d", d);
        tpl.bind("obj", obj_tpl);

        std::string str = "{\"a\": {\"1\":[0,1],\"2\":[3,4]}, \"obj\": {\"c\": 78, \"d\": \"some string\"}}";
        std::istringstream iss(str);

        // Extract values to bound variables
        json::extract(tpl, iss);

        std::cout << "obj.d = " << d << std::endl;
        std::cout << "a[\"1\"].size() = " << a["1"].size() << std::endl;
    }

    std::cout << std::endl;
    std::cout << "** Example 4 - binary data" << std::endl;
    {
        uint8_t* data = reinterpret_cast<uint8_t*>(&main);
        size_t size = 16;

        json::Template tpl;
        tpl.bind("bytes", json::ref_as_raw(data, size));
        json::synthetize(tpl, std::cout, false);
        // {"bytes": "f30f1efa554889e541554154534881ec"}
    }

    std::cout << std::endl << std::endl;
    std::cout << "** Example 5 - POD" << std::endl;
    {
        struct __attribute__((packed)) {
            uint8_t a, b, c, d;
            uint32_t e[3];
        } pod;
        
        json::Template tpl;
        tpl.bind("bytes", json::ref_as_pod(pod));

        std::string str = "{\"bytes\": \"f30f1efa554889e541554154534881ec\"}";
        std::istringstream iss(str);
        json::extract(tpl, iss);

        std::cout << "pod.a = " << std::hex << static_cast<int>(pod.a);
        std::cout << ", pod.e[2] = " << std::hex << pod.e[2] << std::endl;
    }

    return 0;
}

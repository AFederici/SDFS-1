#ifndef UTILS_H
#define UTILS_H
#include <iostream>
#include <string>
#include <vector>
#include <netdb.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

using std::string;
using std::vector;

vector<string> splitString(string s, string delimiter);
string getIP();
string getIP(const char * host);

//adapted from https://stackoverflow.com/questions/23030267/custom-sorting-a-vector-of-tuples
template<int M, template<typename> class F = std::less>
struct TupleCompare
{
    template<typename T>
    bool operator()(T const &t1, T const &t2)
    {
        return F<typename tuple_element<M, T>::type>()(std::get<M>(t1), std::get<M>(t2));
    }
};


#endif //UDPSOCKET_H

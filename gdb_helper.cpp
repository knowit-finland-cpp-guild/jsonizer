#include <iostream>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <thread>

using HiResClock=std::chrono::high_resolution_clock;
using Millisecs=std::chrono::milliseconds;

void showts(std::chrono::time_point<std::chrono::system_clock> const& t)
{
static std::string format{"UTC: %d.%m.%Y %H:%M:%S"};
auto tc{std::chrono::system_clock::to_time_t(t)};
auto tt{*std::localtime(&tc)};
std::stringstream ss;
ss << std::put_time(&tt,format.c_str())
   << '.' << std::setfill('0') << std::setw(3)
   << (t.time_since_epoch()%1000).count();
std::cout << ss.str() << std::endl;
}

int main()
{
auto t0{HiResClock::now()};
std::this_thread::sleep_for(std::chrono::milliseconds(1000));
auto t1{HiResClock::now()};
std::cout << "took " << std::chrono::duration_cast<Millisecs>(t1-t0).count()
    << std::endl;
}

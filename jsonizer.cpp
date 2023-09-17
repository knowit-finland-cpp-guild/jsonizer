/**
JSON object factory.

VERSION 1.0

This is a threading and other C++ features demo application that acts
as a factory for JSON data.

The logic of the application models a kind of a "parts and products" processing
factory (or factories).

A Producer thread generates semi random JSON key-value pairs, here
called Parts, in the fashion of "basename_a":<value> ... "basename_zzz":<value>,
where values are basic JSON value types of ints, doubles and strings.

These Parts get pushed into a queue.

Then consumer factories within the same thread are given the chance of getting
these Parts from the queue in order to form larger Parts consisting of
JSON arrays and objects, provided that the queue head Part meets the required
criteria (e.g. "array of ints" factory only accepts int kvpairs).
These more complex Parts in turn can be pushed back to the queue to form
even larger Parts.

This process is a kind of a modeling of an assembly line.
All types of Parts can be determined to be final products, in which case they
get moved into another queue and become Products.

The Products in turn are consumed by Assembly threads, which form the final
JSON objects, one per Assembly thread.

The setup enables creating large and complex JSON objects.

The criteria for a complete JSON object is that the count of simple JSON
compatible values (integers, doubles and strings) equals or exceeds the given
parameters. At Assembly thread startup, the thread adds the requested amount of
simple values to the work queue of the Producer. However, these values aren't
"earmarked" to that particular Assembly thread but instead can be consumed by
any other Assembly thread as well. As the consuming of the Products consists of
simple key-value pairs as well as arrays, objects, or multi-dimensional
combinations of those, an Assembly thread will likely consume more values than
it originally fed to the work queue, thus potentially starving other Assembly
threads, or the not-ready Parts (like arrays, which have predetermined min/max
ranges for their sizes) within the Producer thread. Thus, when an Assembly
thread can't seem to receive new Products, it feeds some more values to the
system.

The parametrization of the Producer object greatly affects what kind of
JSON objects get created.

This application is kind of meant to act as a service, i.e. clients request
JSON data, and the data then gets generated parallelly per request for fast
response time, but the service triggering and response giving mechanisms
(e.g. REST API) have yet to be implemented.
*/

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <set>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

using namespace std::chrono_literals;

namespace
{
using V_S=std::vector<std::string>;
using V_I=std::vector<int>;
using V_D=std::vector<double>;

using D_S=std::deque<std::string>;
using D_I=std::deque<int>;
using D_D=std::deque<double>;

using D_S_Ptr=std::shared_ptr<D_S>;
using D_I_Ptr=std::shared_ptr<D_I>;
using D_D_Ptr=std::shared_ptr<D_D>;

using OptString=std::optional<std::string>;
using Key=OptString;
using Value=OptString;

using Serial=int;

const std::size_t DEFAULT_MINSIZE{1};
const std::size_t DEFAULT_MAXSIZE{2};
const std::size_t DEFAULT_RECIRC{50};
const std::size_t DEFAULT_WEIGTH{1};

enum class CT
{
 KI,KD,KS
,AI,AD,AS,AA,AO,AM
,OI,OD,OS,OA,OO,OM
};

enum ERRORS
{
NO
, USAGE
, CMDLINE_INVALID_PREDEFINED
, CMDLINE_EXCEPTION
};

std::mutex muxCvProd,muxCvAsse,muxLog;
std::condition_variable cvProd,cvAsse;

void print(std::string const& s)
{
std::cout << s << std::endl;
}

#define LOG(data) ({std::lock_guard lock(muxLog); \
std::stringstream ss__LINE__; ss__LINE__ << data; print(ss__LINE__.str());})

V_S g_substantives {
"abaxiator","adscititiouser","affranchiser","aoristicor","athwarter"
,"beaconacor","bheestier","biconcaver","blitherer","buckrammer"
,"centuplicator","chicanerer","coarticulor","cribriformer","ctenidiumer"
,"dactyolizer","delabializator","diminuendor","dubitator","dwindler"
,"eccentrizer","elasticizer","enantiotrophier","eosinophiler","equiprobabilizer"
,"fenestrator","firnificator","flagellator","foliculator","foppisher"
,"gesticulator","ghoulizer","gimcrackerizer","glaciator","gobbledegooker"
,"haplographier","hemistitcher","hierarchizer","horologizer","hyalogizer"
,"illminator","inviolator","iotacer","isomorpher","itemizer"
,"jangler","jettisoner","jibber","jotter","jurisprudenter"
,"katamorpher","kinaestethor","knaverer","kottabosser","kyoodler"
,"laborizer","legitimizer","ligaturer","listlessor","locator"
,"maculator","merchandizor","mimesizer","modalator","multifarier"
,"namablor","negligor","nicher","nocturner","nuncupator"
,"oblanceolator","octamerer","officializer","omitter","oxymoronizer"
,"parasynthesizer","pedimentor","phantastronizer","pickler","plagiotropisizer"
,"quacker","quaererizor","quantumizer","quarreler","quaternator"
,"rachiformer","readjustor","rinser","rollicker","ruinator"
,"salienator","scatterer","segmentalizer","shaper","sinuouser"
,"tanstaafler","tediumizer","thougher","tillyvallier","toilsomizer"
,"ubiquitter","ultimator","umbriferouser","unconformer","upsurger"
,"valuator","vehiculumizer","vinculumizer","vorticer","vulganizer"
,"wackier","whammier","wiggler","wreather","wrought-upper"
,"xanthiciser","xerarchizer","x-unitizer","xylographer","xylotomizer"
,"yarner","yerker","yielder","yonderer","yummizer"
,"zagger","zanizer","zonator","zoomer","zymosizer"};

Serial serialGenerator{};

std::random_device rd;
std::mt19937 mt{rd()};

template<typename T, typename U>
std::tuple<T&,U> tie2(T&& t, U&& u)
{
return {t,u};
}

template<typename T, typename U, typename V>
std::tuple<T&,U,V> tie3(T&& t, U&& u, V&&v)
{
return {t,u,v};
}

template<typename T, typename U, typename V, typename W>
std::tuple<T&,U,V,W> tie4(T&& t, U&& u, V&& v, W&& w)
{
return {t,u,v,w};
}

template<typename T, typename U, typename V, typename W, typename X>
std::tuple<T&,U,V,W,X> tie5(T&& t, U&& u, V&& v, W&& w, X&& x)
{
return {t,u,v,w,x};
}

template<std::size_t N,typename T> struct Tuple
{
template<typename U> static void init(U& t)
{
std::get<0>(t)=std::make_shared<T>();
}
};

template<typename T> struct Tuple<2,T>
{
template<typename U> static void init(U& t)
{
std::get<0>(t)=std::make_shared<T>(std::get<1>(t));
}
};

template<typename T> struct Tuple<3,T>
{
template<typename U> static void init(U& t)
{
std::get<0>(t)=std::make_shared<T>(std::get<1>(t),std::get<2>(t));
}
};

template<typename T> struct Tuple<4,T>
{
template<typename U> static void init(U& t)
{
std::get<0>(t)=std::make_shared<T>(
    std::get<1>(t),std::get<2>(t),std::get<3>(t));
}
};

template<typename T> struct Tuple<5,T>
{
template<typename U> static void init(U& t)
{
std::get<0>(t)=std::make_shared<T>(
    std::get<1>(t),std::get<2>(t),std::get<3>(t),std::get<4>(t));
}
};

auto init{[](auto&&... t)
    {
    auto impl{[](auto&& me, auto&& head, auto&&... tail)
        {
        using T=typename std::remove_reference_t<
            std::tuple_element_t<0,
                std::remove_reference_t<decltype(head)>>>::element_type;

        Tuple<std::tuple_size_v<
            std::remove_reference_t<decltype(head)>>,T>::init(head);

        if constexpr(sizeof...(tail)>0)
            me(me,tail...);
    }};
    impl(impl,t...);
    }
};
} // unnamed

//------------------------------------------------------------------------------
template<typename T=std::string>
class DisNDat
{
public:

DisNDat(T const& dis, T const& dat)
    : mDis(dis)
    , mDat(dat)
{}

T const& get() const
{
    if(!mDissed)
    {
        mDissed=true;
        return mDis;
    }
    return mDat;
}

private:

    T mDis;
    T mDat;
    mutable bool mDissed{};

friend std::ostream& operator<<(std::ostream& os, DisNDat const& rhs)
{
    os << rhs.get();
    return os;
}
};

//------------------------------------------------------------------------------
template<typename T> std::string conv(T& t)
{
return std::to_string(t);
}

std::string conv(std::string const& t)
{
return "\"" + t + "\"";
}

template<typename T> std::string getFrom(
    T const& t,
    std::size_t start,
    std::size_t size)
{
    auto ix{std::min(t.size()-1,start+mt()%size)};
    return conv(t[ix]);
}

//------------------------------------------------------------------------------
struct Part;
using PartPtr=std::shared_ptr<Part>;
using D_PartPtr=std::deque<PartPtr>;
using OptD_PartPtr=std::optional<D_PartPtr>;

struct Part
{
enum class SimpleType
    {
     INT
    ,DOUBLE
    ,STRING
    };

enum class Type
    {
     INT
    ,DOUBLE
    ,STRING
    ,ARRAY
    ,OBJECT
    };

static Type T2T(SimpleType const& type)
{
return type==SimpleType::INT
    ? Type::INT
    : type==SimpleType::DOUBLE
        ? Type::DOUBLE
        : Type::STRING;
}

bool isSimple() const
{
return mType==Type::INT || mType==Type::DOUBLE || mType==Type::STRING;
}

Part(
    Type type
    ,Key const& key
    ,Value const& val=OptString()
    ,PartPtr sub=nullptr)
    : mSerial(++serialGenerator)
    , mType(type)
    , mKey(key)
    , mValue(val)
    , mValueCount(isSimple() ? 1 : 0)
{
if(sub)
    mSubs=D_PartPtr{sub};
}

Part(
    Type type
    ,OptD_PartPtr const& subs
    ,Key const& key
    ,Value const& val=OptString())
    : mSerial(++serialGenerator)
    , mType(type)
    , mKey(key)
    , mValue(val)
    , mSubs(subs)
    , mValueCount(isSimple() ? 1 : 0)
{}

explicit Part(int val, OptString const& key=OptString())
    : mSerial(++serialGenerator)
    , mType(Type::INT)
    , mKey(key)
    , mValue(conv(val))
    , mValueCount(1)
{}

explicit Part(double val, OptString const& key=OptString())
    : mSerial(++serialGenerator)
    , mType(Type::DOUBLE)
    , mKey(key)
    , mValue(conv(val))
    , mValueCount(1)
{}

explicit Part(std::string const& val, OptString const& key=OptString())
    : mSerial(++serialGenerator)
    , mType(Type::STRING)
    , mKey(key)
    , mValue(conv(val))
    , mValueCount(1)
{}

bool match(SimpleType type) const
{
return type==SimpleType::INT
    ? mType==Type::INT
    : type==SimpleType::DOUBLE
        ? mType==Type::DOUBLE
        : mType==Type::STRING;
}

std::size_t valueCount(SimpleType type) const
{
if(isSimple())
    return mType==T2T(type) ? mValueCount : 0;

std::size_t count{};
if(mSubs)
    for(auto const& i: *mSubs)
        count+=i->valueCount(type);

return count;
}

Type type() const
{
return mType;
}

Key const& key() const
{
return mKey;
}

void setKey(Key const& key)
{
mKey=key;
}

Value const& value() const
{
return mValue;
}

OptD_PartPtr& subs()
{
return mSubs;
}

Serial const& serial() const
{
return mSerial;
}

private:

Serial mSerial;
Type mType;
Key mKey;
Value mValue;
OptD_PartPtr mSubs;
std::size_t mValueCount{};

friend std::ostream& operator<<(std::ostream& os, Part const& rhs)
{
if(rhs.mKey)
    os << *rhs.mKey << ':';

switch(rhs.mType)
    {
    case Type::INT:
    case Type::DOUBLE:
    case Type::STRING:
        os << *rhs.mValue;
        break;
    case Type::ARRAY:
        os << '[';
        if(rhs.mSubs)
            {
            DisNDat<> c("",",");
            for(auto& i:*rhs.mSubs)
                if(i)
                    os << c << *i;
            }
        os << ']';
        break;
    case Type::OBJECT:
        os << '{';
        if(rhs.mSubs)
            {
            DisNDat<> c{"",","};
            for(auto& i:*rhs.mSubs)
                if(i)
                    os << c << *i;
            }
        os << '}';
    }
return os;
}

friend std::ostream& operator<<(std::ostream& os, Part::Type const& rhs)
{
switch(rhs)
    {
    case Part::Type::INT:
        os << "INT";
        break;
    case Part::Type::DOUBLE:
        os << "DOUBLE";
        break;
    case Part::Type::STRING:
        os << "STRING";
        break;
    case Part::Type::ARRAY:
        os << "ARRAY";
        break;
    case Part::Type::OBJECT:
        os << "OBJECT";
    }
return os;
}
};

//------------------------------------------------------------------------------
struct MuxParts
{
void push_back(PartPtr p)
{
std::unique_lock lock(mMux);
mParts.push_back(p);
}

void pop_front()
{
std::unique_lock lock(mMux);
mParts.pop_front();
}

bool empty()
{
std::shared_lock lock(mMux);
return mParts.empty();
}

std::size_t size()
{
std::shared_lock lock(mMux);
return mParts.size();
}

PartPtr const& front()
{
std::shared_lock lock(mMux);
return mParts.front();
}

PartPtr get()
{
std::unique_lock lock(mMux);
if(mParts.empty())
    return PartPtr();

auto part{mParts.front()};
mParts.pop_front();
return part;
}

private:

std::shared_mutex mMux;
D_PartPtr mParts;
};

//------------------------------------------------------------------------------
template<std::size_t BASE> struct BaseN
{
explicit BaseN(char zero)
    : mZero(zero)
{}

BaseN& operator++()
{
++mVal;
return *this;
}

BaseN operator++(int)
{
BaseN base{*this};
++mVal;
return base;
}

std::string operator*()
{
auto val{mVal};
std::string s;
while(val)
    {
    auto digit{static_cast<std::size_t>(val%BASE)};
    val=(val-digit)/BASE;
    s=std::string(1,mZero+digit)+s;
    }
return s;
}

private:

char mZero;
std::size_t mVal{};
};

//------------------------------------------------------------------------------
struct KeyGetterBase
{
using Token=std::size_t;

virtual ~KeyGetterBase() = default;
virtual std::size_t keyCount(Token) const = 0;
virtual std::string get(Token) const = 0;

virtual Token reg()
{
return 0;
}

virtual void activate(){}
};

using KeyGetterBasePtr = std::shared_ptr<KeyGetterBase>;

//------------------------------------------------------------------------------
struct KeyGetter : public KeyGetterBase
{
KeyGetter(V_S names, std::size_t count)
{
if(!count)
    mKeys.swap(names);
else
    {
    mKeys=names;
    BaseN<1+'z'-'a'> b{'a'};
    for(; count; --count, ++b)
        {
        auto s{*b};
        if(!s.empty())
            s="_"+s;
        for(auto const& i: names)
            mKeys.emplace_back(i+s);
        }
    }
mSlice=mKeys.size();
}

std::size_t keyCount(Token tok) const override
{
auto next{tok*mSlice+mSlice};
auto tail{mKeys.size()-next};
return tail>0 && tail<mSlice ? mSlice+tail : mSlice;
}

std::string get(Token tok) const override
{
return getFrom(mKeys,tok*mSlice,keyCount(tok));
}

Token reg() override
{
return mToken++;
}

virtual void activate()
{
mSlice=mKeys.size()/mToken;
}

private:

V_S mKeys;
Token mToken{};
std::size_t mSlice;
};

//------------------------------------------------------------------------------
template<typename T> struct SimpleValueGenerator
{
using DPtr=std::shared_ptr<std::deque<T>>;

SimpleValueGenerator(DPtr&& values)
    : mValues(values)
{}

PartPtr get() const
{
if(!mValues || mValues->empty())
    return PartPtr();

return std::make_shared<Part>((*mValues)[mt()%mValues->size()]);
}

private:

DPtr mValues;
};

//------------------------------------------------------------------------------
struct FactoryBase
{
virtual ~FactoryBase()=default;
virtual PartPtr get(MuxParts& queue)=0;
};

using FactoryBasePtr=std::shared_ptr<FactoryBase>;

//------------------------------------------------------------------------------
template<Part::SimpleType N> struct SimpleKvPairFactory
    : public FactoryBase
{
using Ptr=std::shared_ptr<SimpleKvPairFactory>;

SimpleKvPairFactory(KeyGetterBasePtr keys)
    : mpKeys(keys)
{
if(keys)
    mTok=keys->reg();
}

PartPtr get(MuxParts& queue) override
{
if(queue.empty())
    return PartPtr();

auto part{queue.front()};
if(!part)
    return PartPtr();

queue.pop_front();
auto keys{mpKeys.lock()};
if(keys && part->match(N) && !part->key() && part->value())
    return std::make_shared<Part>(Part::T2T(N),keys->get(mTok),part->value());

return PartPtr();
}

private:

std::weak_ptr<KeyGetterBase> mpKeys;
KeyGetter::Token mTok{};
};

//------------------------------------------------------------------------------
struct ContainerFactoryBase : public FactoryBase
{
ContainerFactoryBase(
    std::size_t minLen,
    std::size_t maxLen,
    bool autoClear,
    Part::Type partType,
    KeyGetterBasePtr keys)
    : mMinLen(minLen)
    , mMaxLen(maxLen)
    , mExpectedLen(maxLen<=minLen ? minLen : (mt()%(1+maxLen-minLen)+minLen))
    , mAutoClear(autoClear)
    , mPartType(partType)
    , mpKeys(keys)
{
if(keys)
    mTok=keys->reg();
}

virtual bool match(PartPtr p) const=0;

PartPtr get(MuxParts& queue) override
{
if(mSubs.size()<mExpectedLen)
    {
    PartPtr p;
    if(!queue.empty())
        p=queue.front();

    if(match(p))
        {
        if(mPartType==Part::Type::ARRAY)
            p->setKey(Key());
        mSubs.push_back(p);
        queue.pop_front();
        }
    if(mSubs.size()<mExpectedLen)
        return PartPtr();
    }
auto keys{mpKeys.lock()};
if(!keys)
    return PartPtr();

auto part{std::make_shared<Part>(mPartType,mSubs,keys->get(mTok))};
mExpectedLen=mMaxLen<=mMinLen ? mMinLen : (mt()%(1+mMaxLen-mMinLen)+mMinLen);
if(mAutoClear)
    mSubs.clear();

return part;
}

bool uniqueKey(std::string const& rhs) const
{
for(auto const& i: mSubs)
    if(i && i->key() && *(i->key())==rhs)
        return false;

return true;
}

private:

D_PartPtr mSubs;
std::size_t mMinLen;
std::size_t mMaxLen;
std::size_t mExpectedLen;
bool mAutoClear{};
Part::Type mPartType;
std::weak_ptr<KeyGetterBase> mpKeys;
KeyGetter::Token mTok{};
};

//------------------------------------------------------------------------------
template<Part::SimpleType N> struct SimpleArrayFactory
    : public ContainerFactoryBase
{
using Ptr=std::shared_ptr<SimpleArrayFactory>;

SimpleArrayFactory(
    std::size_t minLen,
    std::size_t maxLen,
    bool autoClear,
    KeyGetterBasePtr keys)
    : ContainerFactoryBase(minLen,maxLen,autoClear,Part::Type::ARRAY,keys)
{}

bool match(PartPtr p) const override
{
return p && p->match(N) && !p->key() && p->value();
}
};

//------------------------------------------------------------------------------
template<Part::SimpleType N> struct SimpleObjectFactory
    : public ContainerFactoryBase
{
using Ptr=std::shared_ptr<SimpleObjectFactory>;

SimpleObjectFactory(
    std::size_t minLen,
    std::size_t maxLen,
    bool autoClear,
    KeyGetterBasePtr keys)
    : ContainerFactoryBase(minLen,maxLen,autoClear,Part::Type::OBJECT,keys)
{}

bool match(PartPtr p) const override
{
return p && p->match(N) && p->key() && p->value() && uniqueKey(*(p->key()));
}
};

//------------------------------------------------------------------------------
struct ObjectArrayFactory : public ContainerFactoryBase
{
using Ptr=std::shared_ptr<ObjectArrayFactory>;

ObjectArrayFactory(
    std::size_t minLen,
    std::size_t maxLen,
    bool autoClear,
    KeyGetterBasePtr keys)
    : ContainerFactoryBase(minLen,maxLen,autoClear,Part::Type::ARRAY,keys)
{}

bool match(PartPtr p) const override
{
return p && p->type()==Part::Type::OBJECT;
}
};

//------------------------------------------------------------------------------
struct ArrayArrayFactory : public ContainerFactoryBase
{
using Ptr=std::shared_ptr<ArrayArrayFactory>;

ArrayArrayFactory(
    std::size_t minLen,
    std::size_t maxLen,
    bool autoClear,
    KeyGetterBasePtr keys)
    : ContainerFactoryBase(minLen,maxLen,autoClear,Part::Type::ARRAY,keys)
{}

bool match(PartPtr p) const override
{
return p && p->type()==Part::Type::ARRAY && p->subs()
   && (p->subs()->empty()
       || ((*p->subs())[0]
           && (*p->subs())[0]->type()!=Part::Type::ARRAY));
}
};

//------------------------------------------------------------------------------
struct MixedArrayFactory : public ContainerFactoryBase
{
using Ptr=std::shared_ptr<MixedArrayFactory>;

MixedArrayFactory(
    std::size_t minLen,
    std::size_t maxLen,
    bool autoClear,
    KeyGetterBasePtr keys)
    : ContainerFactoryBase(minLen,maxLen,autoClear,Part::Type::ARRAY,keys)
{}

bool match(PartPtr p) const override
{
return p!=nullptr;
}
};

//------------------------------------------------------------------------------
struct ArrayObjectFactory : public ContainerFactoryBase
{
using Ptr=std::shared_ptr<ArrayObjectFactory>;

ArrayObjectFactory(
    std::size_t minLen,
    std::size_t maxLen,
    bool autoClear,
    KeyGetterBasePtr keys)
    : ContainerFactoryBase(minLen,maxLen,autoClear,Part::Type::OBJECT,keys)
{}

bool match(PartPtr p) const override
{
return p && p->key()  && uniqueKey(*(p->key())) && p->type()==Part::Type::ARRAY;
}
};

//------------------------------------------------------------------------------
struct ObjectObjectFactory : public ContainerFactoryBase
{
using Ptr=std::shared_ptr<ObjectObjectFactory>;

ObjectObjectFactory(
    std::size_t minLen,
    std::size_t maxLen,
    bool autoClear,
    KeyGetterBasePtr keys)
    : ContainerFactoryBase(minLen,maxLen,autoClear,Part::Type::OBJECT,keys)
{}

bool match(PartPtr p) const override
{
return p && p->key() && uniqueKey(*(p->key())) && p->type()==Part::Type::OBJECT;
}
};

//------------------------------------------------------------------------------
struct MixedObjectFactory : public ContainerFactoryBase
{
using Ptr=std::shared_ptr<MixedObjectFactory>;

MixedObjectFactory(
    std::size_t minLen,
    std::size_t maxLen,
    bool autoClear,
    KeyGetterBasePtr keys)
    : ContainerFactoryBase(minLen,maxLen,autoClear,Part::Type::OBJECT,keys)
{}

bool match(PartPtr p) const override
{
return p && p->key() && uniqueKey(*(p->key()));
}
};

//------------------------------------------------------------------------------
struct ProducerParams
{
struct ConsumerParams
{
std::size_t min{};
std::size_t max{};
std::size_t recirc{};
std::size_t weigth{};
};

using M_ConsumerParams=std::map<CT,ConsumerParams>;
using D_D_I=std::vector<D_I>;
using D_D_D=std::vector<D_D>;
using D_D_S=std::vector<D_S>;

D_D_I ints() const
{
return mInts;
}

D_D_D doubles() const
{
return mDoubles;
}

D_D_S strings() const
{
return mStrings;
}

void addInts(D_I const& rhs)
{
mInts.push_back(rhs);
}

void addDoubles(D_D const& rhs)
{
mDoubles.push_back(rhs);
}

void addStrings(D_S const& rhs)
{
mStrings.push_back(rhs);
}

ConsumerParams const& operator[](CT i)
{
return mCons[i];
}

void setKeys(V_S keys)
{
mKeys.swap(keys);
}

void setKeyMultiplier(std::size_t rhs)
{
mMultiplier=rhs;
}

V_S const& keys() const
{
return mKeys;
}

std::size_t keyMultiplier() const
{
return mMultiplier;
}

void setConsumerParams(M_ConsumerParams rhs)
{
mCons.swap(rhs);
}

void setConsumerParam(CT ct, ConsumerParams const& cp)
{
mCons[ct]=cp;
}

private:

V_S mKeys;
std::size_t mMultiplier{};
D_D_I mInts;
D_D_D mDoubles;
D_D_S mStrings;
M_ConsumerParams mCons;
};

//------------------------------------------------------------------------------
struct Producer
{
enum IX
{
 FACTORY
,PERCENTAGE
,WEIGTH
};

public:

Producer(ProducerParams par)
{
for(auto const& i: par.ints())
    mValueFIs.push_back(D_I_Ptr{new D_I{i}});

for(auto const& i: par.doubles())
    mValueFDs.push_back(D_D_Ptr{new D_D{i}});

for(auto const& i: par.strings())
    mValueFSs.push_back(D_S_Ptr{new D_S{i}});

mKeyGetter=std::make_shared<KeyGetter>(par.keys(),par.keyMultiplier());

init(
     tie2(mKvpFI,mKeyGetter)
    ,tie2(mKvpFD,mKeyGetter)
    ,tie2(mKvpFS,mKeyGetter)
    ,tie5(mArrayFI,par[CT::AI].min,par[CT::AI].max,true,mKeyGetter)
    ,tie5(mArrayFD,par[CT::AD].min,par[CT::AD].max,true,mKeyGetter)
    ,tie5(mArrayFS,par[CT::AS].min,par[CT::AS].max,true,mKeyGetter)
    ,tie5(mObjectFI,par[CT::OI].min,par[CT::OI].max,true,mKeyGetter)
    ,tie5(mObjectFD,par[CT::OD].min,par[CT::OD].max,true,mKeyGetter)
    ,tie5(mObjectFS,par[CT::OS].min,par[CT::OS].max,true,mKeyGetter)
    ,tie5(mObjArray,par[CT::AO].min,par[CT::AO].max,true,mKeyGetter)
    ,tie5(mArrayArray,par[CT::AA].min,par[CT::AA].max,true,mKeyGetter)
    ,tie5(mMixedArray,par[CT::AM].min,par[CT::AM].max,true,mKeyGetter)
    ,tie5(mArrayObj,par[CT::OA].min,par[CT::OA].max,true,mKeyGetter)
    ,tie5(mObjObj,par[CT::OO].min,par[CT::OO].max,true,mKeyGetter)
    ,tie5(mMixedObj,par[CT::OM].min,par[CT::OM].max,true,mKeyGetter));

mKeyGetter->activate();

mConsumers={
     {mKvpFI,par[CT::KI].recirc,par[CT::KI].weigth}
    ,{mKvpFD,par[CT::KD].recirc,par[CT::KD].weigth}
    ,{mKvpFS,par[CT::KS].recirc,par[CT::KS].weigth}

    ,{mArrayFI,par[CT::AI].recirc,par[CT::AI].weigth}
    ,{mArrayFD,par[CT::AD].recirc,par[CT::AD].weigth}
    ,{mArrayFS,par[CT::AS].recirc,par[CT::AS].weigth}

    ,{mObjectFI,par[CT::OI].recirc,par[CT::OI].weigth}
    ,{mObjectFD,par[CT::OD].recirc,par[CT::OD].weigth}
    ,{mObjectFS,par[CT::OS].recirc,par[CT::OS].weigth}

    ,{mObjArray,par[CT::AO].recirc,par[CT::AO].weigth}
    ,{mArrayArray,par[CT::AA].recirc,par[CT::AA].weigth}
    ,{mMixedArray,par[CT::AM].recirc,par[CT::AM].weigth}

    ,{mArrayObj,par[CT::OA].recirc,par[CT::OA].weigth}
    ,{mObjObj,par[CT::OO].recirc,par[CT::OO].weigth}
    ,{mMixedObj,par[CT::OM].recirc,par[CT::OM].weigth}
    };
}

void order(int ints, int doubles, int strings)
{
if(ints>0)
    {
    auto ix{mt()%mValueFIs.size()};
    for(; ints>=0; --ints)
        mParts.push_back(mValueFIs[ix].get());
    }
if(doubles>0)
    {
    auto ix{mt()%mValueFDs.size()};
    for(; doubles>=0; --doubles)
        mParts.push_back(mValueFDs[ix].get());
    }
if(strings>0)
    {
    auto ix{mt()%mValueFSs.size()};
    for(; strings>=0; --strings)
        mParts.push_back(mValueFSs[ix].get());
    }
}

void recirculate(PartPtr p)
{
if(p)
    mParts.push_back(p);
}

void done()
{
mDone=true;
}

PartPtr get()
{
return mProducts.get();
}

std::string produce()
{
std::vector<std::size_t> key,key2;
for(std::size_t i=0; i<mConsumers.size(); ++i)
    for(std::size_t j=0; j<std::get<IX::WEIGTH>(mConsumers[i]); ++j)
        key2.push_back(i);

key=key2;
std::size_t madeProducts{};
std::map<Part::Type,std::size_t> madeTypes;
while(!mDone)
    {
    if(mParts.empty())
        {
        std::unique_lock lock{muxCvProd};
        cvProd.wait(lock,[&mParts=mParts,&mDone=mDone]
            {
            return !mParts.empty() || mDone;
            });
        continue;
        }
    auto candidate{mParts.front()->serial()};
    while(!key.empty())
        {
        auto ix{mt() % key.size()};
        auto ixx{key[ix]};
        key.erase(key.begin()+ix);
        auto& consumer{mConsumers[ixx]};
        auto part{std::get<IX::FACTORY>(consumer)->get(mParts)};
        if(part)
            {
            if(100-std::get<IX::PERCENTAGE>(consumer) < mt()%100)
                mParts.push_back(part);
            else
                {
                ++madeTypes[part->type()];
                mProducts.push_back(part);
                    {
                    std::lock_guard lock{muxCvAsse};
                    cvAsse.notify_all();
                    }
                if(!(++madeProducts % 100))
                    {
                    DisNDat<> c("",", ");
                    std::stringstream ss;
                    ss << "Products created: " << madeProducts << " (";
                    for(auto const& [k,v]: madeTypes)
                        ss << c << k << ": " << v;

                    ss << "); queue size: " << mParts.size();
                    LOG(ss.str());
                    }
                }
            }
        }
    key=key2;
    if(mParts.empty())
        {
        std::lock_guard lock{muxCvAsse};
        cvAsse.notify_all();
        }
    else if(candidate==mParts.front()->serial())
        {
        ++mMisses[candidate];
        if(mMisses[candidate]>2)
            {
            LOG("NOT CONSUMED: " << *mParts.front());
            mProducts.push_back(mParts.front());
            mParts.pop_front();
            mMisses.erase(candidate);
            if(mParts.empty())
                {
                std::lock_guard lock{muxCvAsse};
                cvAsse.notify_all();
                }
            }
        else
            {
            mParts.push_back(mParts.front());
            mParts.pop_front();
            }
        }
    }
LOG("Total products created: " << madeProducts
    << "\nLeftover queue size: " << mParts.size()
    << "\nLeftover products: ");
DisNDat<> c("",",");
std::stringstream ss;
while(!mProducts.empty())
    {
    ss << c << *mProducts.front();
    mProducts.pop_front();
    }
return ss.str();
}

private:

std::deque<SimpleValueGenerator<int>> mValueFIs;
std::deque<SimpleValueGenerator<double>> mValueFDs;
std::deque<SimpleValueGenerator<std::string>> mValueFSs;

SimpleKvPairFactory<Part::SimpleType::INT>::Ptr mKvpFI;
SimpleKvPairFactory<Part::SimpleType::DOUBLE>::Ptr mKvpFD;
SimpleKvPairFactory<Part::SimpleType::STRING>::Ptr mKvpFS;

SimpleArrayFactory<Part::SimpleType::INT>::Ptr mArrayFI;
SimpleArrayFactory<Part::SimpleType::DOUBLE>::Ptr mArrayFD;
SimpleArrayFactory<Part::SimpleType::STRING>::Ptr mArrayFS;

SimpleObjectFactory<Part::SimpleType::INT>::Ptr mObjectFI;
SimpleObjectFactory<Part::SimpleType::DOUBLE>::Ptr mObjectFD;
SimpleObjectFactory<Part::SimpleType::STRING>::Ptr mObjectFS;

ObjectArrayFactory::Ptr mObjArray;
ArrayArrayFactory::Ptr mArrayArray;
ArrayArrayFactory::Ptr mMixedArray;

ArrayObjectFactory::Ptr mArrayObj;
ObjectObjectFactory::Ptr mObjObj;
ObjectObjectFactory::Ptr mMixedObj;

// Tuple items:                   factory       ,recirc% ,weigth*
using ConsumerProducer=std::tuple<FactoryBasePtr,unsigned,unsigned>;
std::vector<ConsumerProducer> mConsumers;

MuxParts mParts;
MuxParts mProducts;
std::map<Serial,int> mMisses;
KeyGetterBasePtr mKeyGetter;
std::atomic<bool> mDone{};
};

//------------------------------------------------------------------------------
struct Assembly
{
Assembly(
    std::shared_ptr<Producer> prod,
    int ints,
    int doubles,
    int strings)
    : mProd(prod)
    , mInts(ints)
    , mDoubles(doubles)
    , mStrings(strings)
{}

std::string run()
{
auto beg{std::chrono::steady_clock::now()};
mProd->order(mInts,mDoubles,mStrings);
{
std::lock_guard lock{muxCvProd};
cvProd.notify_one();
}
{
std::unique_lock lock{muxCvAsse};
cvAsse.wait(lock);
}
int iCount{};
int dCount{};
int sCount{};
std::stringstream ss;
ss << '{';
DisNDat<> c("",",");
std::map<std::string,Part::Type> keys;
while((iCount<mInts || dCount<mDoubles || sCount<mStrings))
    {
    auto prod{mProd->get()};
    if(!prod)
        {
        mProd->order(mInts-iCount>0 ? 1:0,
            mDoubles-dCount>0 ? 1:0,mStrings-sCount>0 ? 1:0);
        {
        std::lock_guard lock{muxCvProd};
        cvProd.notify_one();
        }
        std::unique_lock lock{muxCvAsse};
        cvAsse.wait(lock);
        continue;
        }
    bool recirc{};
    if(!prod->key())
        recirc=true;
    else
        {
        if(keys.find(*prod->key())!=keys.end())
            recirc=true;
        else
            keys[*prod->key()]=prod->type();
        }
    if(recirc)
        {
            LOG("recirc object: " << *prod->key()
                << " type: " << prod->type()
                << " serial: " << prod->serial());

        mProd->recirculate(prod);
        continue;
        }
    ss << c << *prod;
    iCount+=prod->valueCount(Part::SimpleType::INT);
    dCount+=prod->valueCount(Part::SimpleType::DOUBLE);
    sCount+=prod->valueCount(Part::SimpleType::STRING);
    }
ss << '}';
auto s{ss.str()};
auto end{std::chrono::steady_clock::now()};
auto t{std::chrono::duration_cast<std::chrono::nanoseconds>(end-beg).count()};
double d{1.0*t/1000000.0};
LOG("Created [" << mInts << ',' << mDoubles << ','
    << mStrings << "] in " << d << " ms for JSON of size: " << s.size());
return s;
}

private:

std::shared_ptr<Producer> mProd;
int mInts{};
int mDoubles{};
int mStrings{};
};

//------------------------------------------------------------------------------
void initProducerKeys(ProducerParams& pp)
{
pp.setKeys(g_substantives);
}

//------------------------------------------------------------------------------
void initProducerKeysMultiplier(ProducerParams& pp)
{
pp.setKeyMultiplier(26*26*2);
}

//------------------------------------------------------------------------------
void initProducerValues(ProducerParams& pp)
{
pp.addInts({0,1,2,3,4,5,6,7,8,9,});
pp.addInts({10,11,12,13,14,15,16,17,18,19,});
pp.addInts({110,111,112,113,114,115,116,117,118,119,});
pp.addDoubles({0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0,});
pp.addDoubles({10.1,10.2,10.3,10.4,10.5,10.6,10.7,10.8,10.9,11.0,});
pp.addDoubles({110.11,110.21,110.31,110.41,110.15,110.61,110.71,110.18
    ,110.19,111.02,});
pp.addStrings({"A-0001","B-0010","C-0100","D-1000","E-1001","F-1010"
    ,"G-1100","H-1101","I-1111",});
pp.addStrings({"3212-ab","4230-bb","4901-cb","9443-db","8444-eg","3300-ff"
    ,"5932-gb","0943-hb","4064-ig",});
}

//------------------------------------------------------------------------------
using V_Counts=std::vector<std::tuple<int,int,int>>;
std::set<std::string> threadize(ProducerParams& pp, V_Counts const& v)
{
std::set<std::string> results;
auto prod{std::make_shared<Producer>(pp)};
auto futProducer{std::async(std::launch::async,
    [&prod]
    {
    auto res{prod->produce()};
    LOG(res);
    })};
std::deque<std::future<std::string>> futs;
for(auto const& i: v)
    futs.push_back(std::async(std::launch::async,
        [&prod,i]
        {Assembly a{prod,std::get<0>(i),std::get<1>(i),std::get<2>(i)};
        return a.run();
        }));

for(auto i{futs.begin()}; i!=futs.end();)
    {
    if(i->wait_for(1ms)==std::future_status::ready)
        {
        auto res{i->get()};
        results.emplace(res);
        std::swap(*i,futs.back());
        if(std::next(i)==futs.end())
            i=futs.begin();

        futs.pop_back();
        if(futs.empty())
            break;
        }
    else
        if(++i==futs.end())
            i=futs.begin();
    }
prod->done();
{
std::lock_guard lock{muxCvProd};
cvProd.notify_one();
}
while(futProducer.wait_for(1ms)!=std::future_status::ready);
return results;
}

//------------------------------------------------------------------------------
void usage()
{
LOG(
R"(jsonizer usage:
-h      : This help
-s [N]  : Keys multiplier, adds e.g. _a ... _zzz postfix
         Example: -s 52
-p [key]: Use predefined config
         Currently valid are: default, godbolt, complex
         Example: -p godbolt
-c [xx,min,max,recirc%,weigth]
         where min, max, recirc% and weigth are numbers
         having defaults of 1, 2, 50, 1, respectively,
         and xx is one of KI,KD,KS, AI,AD,AS,AO,AA,AM, OI,OD,OS,OA,OO,OM,
         representing a specific type of factory. Legend:
         K=keyed, I=integer, D=double, S=string, A=array, O=object,
         M=mixed type values.
         This param can be given several times.
-t [int values,double values,string values]
         This represents one JSON file production constraints, i.e.
         a minimum of this many values of specified type will exist in
         the produced JSON object. Example: -t 100,100,100
         This param can be given several times.)");
}

//------------------------------------------------------------------------------
int parseCmdline(
    int argc,
    char* argv[],
    ProducerParams& pp,
    V_Counts& counts,
    std::map<std::string,ProducerParams> const& predefined)
{
auto splitz{[&](
    auto&& splitz,
    std::vector<std::string>& res,
    std::string const& s,
    std::size_t pos=0) -> void
    {
    if(pos!=std::string::npos)
        {
        auto pos2{s.find(",",pos)};
        res.push_back(s.substr(pos,pos2));
        if(pos2!=std::string::npos)
            splitz(splitz,res,s,++pos2);
        }
    }};
try
    {
    const std::set<std::string> KEYS_1{"-h"};
    const std::set<std::string> KEYS_2{"-s","-p","-c","-t"};
    std::map<std::string,std::vector<std::string>> candidates;
    for(int i=1; i<argc; ++i)
        {
        if(KEYS_2.find(argv[i])!=KEYS_2.end())
            {
            int j=i+1;
            if(!strcmp(argv[i],"-h")
               || (j<argc && KEYS_2.find(argv[j])==KEYS_2.end()))
                candidates[argv[i]].push_back(argv[j]);
            }
        else if(KEYS_1.find(argv[i])!=KEYS_1.end())
            candidates[argv[i]];
        else if(argv[i][0]=='-')
            {
            usage();
            return ERRORS::USAGE;
            }
        }
    auto k{candidates.find("-h")};
    if(k!=candidates.end())
        {
        usage();
        return ERRORS::USAGE;
        }
    k=candidates.find("-s");
    if(k!=candidates.end())
        for(auto i: k->second)
            pp.setKeyMultiplier(std::stoi(i));

    k=candidates.find("-p");
    if(k!=candidates.end())
        for(auto i: k->second)
            {
            auto j{predefined.find(i)};
            if(j!=predefined.end())
                pp=j->second;
            else
                {
                usage();
                return ERRORS::CMDLINE_INVALID_PREDEFINED;
                }
            }
    k=candidates.find("-c");
    if(k!=candidates.end())
        {
        static const std::map<std::string,CT> KEYS{
             {"KI",CT::KI}
            ,{"KD",CT::KD}
            ,{"KS",CT::KS}
            ,{"AI",CT::AI}
            ,{"AD",CT::AD}
            ,{"AS",CT::AS}
            ,{"AO",CT::AO}
            ,{"AA",CT::AA}
            ,{"AM",CT::AM}
            ,{"OI",CT::OI}
            ,{"OD",CT::OD}
            ,{"OS",CT::OS}
            ,{"OA",CT::OA}
            ,{"OO",CT::OO}
            ,{"OM",CT::OM}};
        std::size_t minSize{DEFAULT_MINSIZE};
        std::size_t maxSize{DEFAULT_MAXSIZE};
        std::size_t recirc{DEFAULT_RECIRC};
        std::size_t weigth{DEFAULT_WEIGTH};
        for(auto ii: k->second)
            {
            std::string key;
            std::vector<std::string> v;
            splitz(splitz,v,ii);
            auto vv{v.begin()};
            if(vv!=v.end())
                {
                key=*vv++;
                if(KEYS.find(key)==KEYS.end())
                    continue;
                }
            if(vv!=v.end())
                minSize=std::stoi(*vv++);

            if(vv!=v.end())
                maxSize=std::stoi(*vv++);

            if(vv!=v.end())
                recirc=std::stoi(*vv++);

            if(vv!=v.end())
                weigth=std::stoi(*vv++);

            pp.setConsumerParam(
                KEYS.find(key)->second,{minSize,maxSize,recirc,weigth});
            }
        }
    k=candidates.find("-t");
    if(k!=candidates.end())
        {
        for(auto ii: k->second)
            {
            int i,d,s;
            std::vector<std::string> v;
            splitz(splitz,v,ii);
            i=d=s=0;
            auto vv{v.begin()};
            if(vv!=v.end())
                if(!vv++->empty())
                    d=s=i=std::stoi(*vv);

            if(vv!=v.end())
                if(!vv++->empty())
                    d=s=std::stoi(*vv);

            if(vv!=v.end())
                if(!vv->empty())
                s=std::stoi(*vv);

            counts.push_back({i,d,s});
            }
        }
    }
catch(...)
    {
    DisNDat<> c(""," ");
    std::stringstream ss;
    ss << "\nSomething wrong with the command line arguments: ";
    for(int i=0; i<argc; ++i)
        ss << c << argv[i];
    ss << '\n';
    LOG(ss.str());
    usage();
    return ERRORS::CMDLINE_EXCEPTION;
    }
return 0;
}

std::map<std::string,ProducerParams> initPredefined()
{
ProducerParams pp,pp2;
initProducerKeys(pp2);
initProducerKeysMultiplier(pp2);
initProducerValues(pp2);

using f=std::function<void()>;
static std::map<std::string,f> CONSUMERS{
    {"godbolt",[&pp]{
        pp.setConsumerParams({
             {CT::KI,{0,0,90,1}}
            ,{CT::KD,{0,0,90,1}}
            ,{CT::KS,{0,0,90,1}}
            ,{CT::AI,{4,12,80,1}}
            ,{CT::AD,{3,11,80,1}}
            ,{CT::AS,{2,10,80,1}}
            ,{CT::AO,{2,6,40,1}}
            ,{CT::AA,{3,5,40,1}}
            ,{CT::AM,{2,4,40,1}}
            ,{CT::OI,{3,5,40,1}}
            ,{CT::OD,{4,5,40,1}}
            ,{CT::OS,{2,5,40,1}}
            ,{CT::OA,{4,8,30,1}}
            ,{CT::OO,{3,7,30,1}}
            ,{CT::OM,{2,6,30,1}}
            });}}
    ,{"complex",[&pp]{
        pp.setConsumerParams({
             {CT::KI,{0,0,90,1}}
            ,{CT::KD,{0,0,90,1}}
            ,{CT::KS,{0,0,90,1}}
            ,{CT::AI,{4,12,80,1}}
            ,{CT::AD,{3,11,80,1}}
            ,{CT::AS,{2,10,80,1}}
            ,{CT::AO,{2,6,80,1}}
            ,{CT::AA,{3,5,80,1}}
            ,{CT::AM,{2,4,80,1}}
            ,{CT::OI,{3,5,80,1}}
            ,{CT::OD,{4,5,80,1}}
            ,{CT::OS,{2,5,80,1}}
            ,{CT::OA,{4,8,90,1}}
            ,{CT::OO,{3,7,90,1}}
            ,{CT::OM,{2,6,90,1}}
            });}}
    ,{"default",[&pp]{
        pp.setConsumerParams({
             {CT::KI,{1,2,50,1}}
            ,{CT::KD,{1,2,50,1}}
            ,{CT::KS,{1,2,50,1}}
            ,{CT::AI,{1,2,50,1}}
            ,{CT::AD,{1,2,50,1}}
            ,{CT::AS,{1,2,50,1}}
            ,{CT::AO,{1,2,50,1}}
            ,{CT::AA,{1,2,50,1}}
            ,{CT::AM,{1,2,50,1}}
            ,{CT::OI,{1,2,50,1}}
            ,{CT::OD,{1,2,50,1}}
            ,{CT::OS,{1,2,50,1}}
            ,{CT::OA,{1,2,50,1}}
            ,{CT::OO,{1,2,50,1}}
            ,{CT::OM,{1,2,50,1}}
            });}}};
std::map<std::string,ProducerParams> ppp;
for(auto& [k,v]: CONSUMERS)
    {
    pp=pp2;
    v();
    ppp[k]=pp;
    }
return ppp;
}

int main(int argc, char* argv[])
{
V_Counts counts;
std::map<std::string,ProducerParams> predefined{initPredefined()};
ProducerParams pp{predefined.find("default")->second};
auto r{parseCmdline(argc,argv,pp,counts,predefined)};
if(r)
    exit(r);

static const V_Counts defaultCounts{
     {70,70,70}
    ,{60,60,60}
    ,{50,50,50}
    ,{40,40,40}
    ,{30,30,30}
    ,{20,20,20}
    };
if(counts.empty())
    counts=defaultCounts;

auto beg{std::chrono::steady_clock::now()};
auto results{threadize(pp,counts)};
auto end{std::chrono::steady_clock::now()};
auto t{std::chrono::duration_cast<std::chrono::nanoseconds>(end-beg).count()};
double d{1.0*t/1000000.0};
LOG("RUN took: " << d << " ms");
LOG("created " << results.size() << " JSON files");
for(auto const& i: results)
    LOG("Result: " << i << '\n');
}
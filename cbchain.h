#ifndef _CB_CHAIN_H_
#define _CB_CHAIN_H_

//TODO: optimize callback param pass( use && move semntic), use CB_ARG_PASS
//TODO: DONE!: add multi cbchain callback when_all...
//TODO: DONE!: add cbchain cancel 
//TODO: cbchain_retry need set trigger_->next_ null;

#include <type_traits>
#include <memory> 
#include <vector>

namespace callbackchain{
	
template<typename ReturnType>
class cbchain;
class cbinterface;

namespace helper{
	namespace stdx
	{
		template<class _T>
		_T&& declval();
	}

	template <typename _Ty>
    struct _UnwrapTaskType
    {
        typedef _Ty _Type;
    };

    template <typename _Ty>
    struct _UnwrapTaskType<cbchain<_Ty>>
    {
        typedef _Ty _Type;
    };

	
	template <typename _Type>
    struct _TaskTypeTraits
    {
        typedef typename _UnwrapTaskType<_Type>::_Type _TaskRetType;
    };
	
	template <>
    struct _TaskTypeTraits<void>
    {
        typedef void _TaskRetType;
    };
	
    template <typename _Function> auto _IsCallable(_Function _Func, int) -> decltype(_Func(), std::true_type()) { (void)(_Func); return std::true_type(); }
    template <typename _Function> std::false_type _IsCallable(_Function, ...) { return std::false_type(); }
	
	template<typename _Type>
    cbchain<_Type> _To_task(_Type t);
    
    template<typename _Func>
    cbchain<void> _To_task_void(_Func f);
    
	struct _BadContinuationParamType{};

    template <typename _Function, typename _Type> auto _ReturnTypeHelper(_Type t, _Function _Func, int, int) -> decltype(_Func(_To_task(t)));
    template <typename _Function, typename _Type> auto _ReturnTypeHelper(_Type t, _Function _Func, int, ...) -> decltype(_Func(t));
    template <typename _Function, typename _Type> auto _ReturnTypeHelper(_Type t, _Function _Func, ...) -> _BadContinuationParamType;

    template <typename _Function, typename _Type> auto _IsTaskHelper(_Type t, _Function _Func, int, int) -> decltype(_Func(_To_task(t)), std::true_type());
    template <typename _Function, typename _Type> std::false_type _IsTaskHelper(_Type t, _Function _Func, int, ...);

    template <typename _Function> auto _VoidReturnTypeHelper(_Function _Func, int, int) -> decltype(_Func(_To_task_void(_Func)));
    template <typename _Function> auto _VoidReturnTypeHelper(_Function _Func, int, ...) -> decltype(_Func());

    template <typename _Function> auto _VoidIsTaskHelper(_Function _Func, int, int) -> decltype(_Func(_To_task_void(_Func)), std::true_type());
	
	template<typename _Function, typename _ExpectedParameterType>
    struct _FunctionTypeTraits
    {
        typedef decltype(_ReturnTypeHelper(stdx::declval<_ExpectedParameterType>(),stdx::declval<_Function>(), 0, 0)) _FuncRetType;
        static_assert(!std::is_same<_FuncRetType,_BadContinuationParamType>::value, "incorrect parameter type for the callable object in 'then'; consider _ExpectedParameterType or cbchain<_ExpectedParameterType> (see below)");
    };

    template<typename _Function>
    struct _FunctionTypeTraits<_Function, void>
    {
        typedef decltype(_VoidReturnTypeHelper(stdx::declval<_Function>(), 0, 0)) _FuncRetType;
    };

    template<typename _Function, typename _ReturnType>
    struct _ContinuationTypeTraits
    {
        typedef cbchain<typename _TaskTypeTraits<typename _FunctionTypeTraits<_Function, _ReturnType>::_FuncRetType>::_TaskRetType> _TaskOfType;
    };
}


struct cbexecinfo
{
	cbinterface* current_;
	cbinterface* trigger_;
	
//private:
	bool stop_flag_;
	std::shared_ptr<cbinterface>*  retry_cb_;
};

extern cbexecinfo* get_current_cb();


class cbinterface:public std::enable_shared_from_this<cbinterface>{
public:
	cbinterface():
		multi_cb_index_(-1), inline_flag_(0)
	{ printf("create orig %p\n",this);}
	cbinterface(cbinterface*):
		multi_cb_index_(-1), inline_flag_(0)
	{ printf("create  %p\n",this); }
	
	virtual ~cbinterface(){ printf("destory %p\n",this); }
	
	virtual void invoke(cbinterface*trigger, void*res) = 0;

	inline std::shared_ptr<cbinterface> ref(){ return shared_from_this();}
	inline void setinline(bool flag){ inline_flag_ =flag?1:0; }
	inline bool isinline(){ return inline_flag_==1; }
	
	//then semantic
	inline std::shared_ptr<cbinterface>& getnext(){ return next_;}
	inline void setnext(const std::shared_ptr<cbinterface>& n){
		if(next_)
			abort();
		next_ =n; 
	}
	inline void invokenext(void* res){
		if(next_)
			next_->invoke( this, res );
	}
	
	//when_all semantic
	inline void setidx(int idx){multi_cb_index_ = idx;}
	inline int getidx(){ return multi_cb_index_; }
	
	//move next
	static void move(cbinterface*newcb, cbinterface*oldcb);
protected:
	std::shared_ptr<cbinterface> next_;
	//gcc unpack_when_all order right to left, clang left to right
	// use this idx to avoid this dirty details.
	int  multi_cb_index_ :31 ;
	unsigned int inline_flag_ :1;
};

extern void invoke_begin(cbinterface*impl,cbinterface*trigger,cbexecinfo* oldexecinfo);
extern bool invoke_end(cbexecinfo* oldexecinfo);
#define INVOKE_BEGIN(impl,trigger) \
	cbexecinfo __oldexecinfo; \
	invoke_begin(impl, trigger, &__oldexecinfo)

#define INVOKE_END() \
	do{ \
		if(invoke_end(&__oldexecinfo)) \
			return ; \
	}while(0)
	
#define CB_ARG_PASS(a) (a)

template<typename ReturnType>
class inovkehelper{
public:
	template<typename Func, typename Arg> 
	void invoke(cbinterface*trigger,cbinterface*impl, Func&f, Arg& a){
		INVOKE_BEGIN(impl, trigger );
		ReturnType result(f(CB_ARG_PASS(a)));
		INVOKE_END();
		impl->invokenext(&result);
	}
	template<typename Func> 
	void invoke(cbinterface*trigger,cbinterface*impl, Func&f ){
		INVOKE_BEGIN(impl, trigger );
		ReturnType result(f());
		INVOKE_END();
		impl->invokenext(&result);
	}
};

template<typename ReturnType>
class inovkehelper<cbchain<ReturnType>>{
public:
	template<typename Func, typename Arg> 
	void invoke(cbinterface*trigger, cbinterface*impl,Func&f, Arg& a){
		INVOKE_BEGIN(impl, trigger );
		cbchain<ReturnType> result(f(CB_ARG_PASS(a)));
		INVOKE_END();
		
		cbinterface::move( result.impl_.get(), impl);
		if(result.isinline()){
			result.invoke();
		}
	}
	template<typename Func> 
	void invoke(cbinterface*trigger,cbinterface*impl,Func&f ){
		INVOKE_BEGIN(impl, trigger );
		cbchain<ReturnType> result(f());
		INVOKE_END();
		
		cbinterface::move( result.impl_.get(), impl);
		if(result.isinline()){
			result.invoke();
		}
	}
};

template<>
class inovkehelper<void>{
public:
	template<typename Func, typename Arg> 
	void invoke(cbinterface*trigger,cbinterface*impl, Func&f, Arg& a){
		INVOKE_BEGIN(impl, trigger );
		f(CB_ARG_PASS(a));
		INVOKE_END();
		
		impl->invokenext( nullptr );
	}
	template<typename Func> 
	void invoke(cbinterface*trigger,cbinterface*impl, Func&f ){
		INVOKE_BEGIN(impl, trigger );
		f();
		INVOKE_END();
		
		impl->invokenext( nullptr );
	}
};


template<typename ReturnType, typename Arg, typename Func>
class cbimpl:public cbinterface
{
public:
	cbimpl(Func f, cbinterface* prev):
		cbinterface(prev), cb_(f)
	{}
	cbimpl(Func f):
		cb_(f)
	{}
	
protected:
	Func cb_;
	inovkehelper<ReturnType> invoke_helper_;
	
public:
	void invoke(cbinterface*trigger, void*res ){
		if(res == nullptr)
			return ;
		Arg* a = reinterpret_cast<Arg*>( res );
		invoke_helper_.invoke(trigger, this, cb_, *a);
	}
};
template<typename ReturnType, typename Func>
class cbimpl<ReturnType, void, Func>:public cbinterface
{
public:
	cbimpl(Func f, cbinterface* prev):
		cbinterface(prev), cb_(f)
	{}
	cbimpl(Func f):
		cb_(f)
	{}
	
protected:
	Func cb_;
	inovkehelper<ReturnType> invoke_helper_;
	
public:
	void invoke(cbinterface*trigger, void*){
		invoke_helper_.invoke(trigger, this, cb_);
	}
};




template<typename ReturnType>
class cbchain{
public:
	typedef ReturnType result_type;

	template<typename Func>
	explicit cbchain(Func f):
		impl_( std::make_shared<cbimpl<ReturnType,void,Func>>( f ))
	{
	}
	
	cbchain(const cbchain<ReturnType>&cb):
		impl_(cb.impl_)
	{
	}
	cbchain(cbchain<ReturnType>&&cb):
		impl_(std::move(cb.impl_))
	{
	}
	cbchain<ReturnType>& operator = (const cbchain<ReturnType>&cb){
		impl_ = cb.impl_;
	}
	cbchain<ReturnType>& operator = ( cbchain<ReturnType>&& cb){
		impl_ = std::move(cb.impl_);
	}
	

	cbchain():impl_(nullptr)
	{}
	
	bool invoke(){
		if(impl_){
			impl_->invoke(nullptr, nullptr);
			return impl_->getnext()!=nullptr;
		}
		return false;
	}
	
	inline bool isinline(){ return impl_->isinline();}
	
	template<typename NextFunc>
	auto then(NextFunc&& f) -> typename helper::_ContinuationTypeTraits<NextFunc,ReturnType>::_TaskOfType
	{
		if( isinline() )
			abort();

		typedef typename helper::_FunctionTypeTraits<NextFunc, ReturnType>::_FuncRetType _FuncRetType;
		typedef typename helper::_TaskTypeTraits<_FuncRetType>::_TaskRetType _NextCBReturnType;
		
		cbchain<_NextCBReturnType> cb;
		cb.impl_ = std::make_shared<cbimpl<_FuncRetType, ReturnType, NextFunc>>( 
			std::forward<NextFunc>(f), impl_.get() );
		impl_->setnext( cb.impl_ );
		return cb;
	}
	
	void share_then(cbchain<ReturnType>&other){
		impl_->setnext( other.impl_->getnext() );
	}
	
	std::shared_ptr<cbinterface> impl_;
private:
	
	template<typename Func>
	friend cbchain<typename helper::_FunctionTypeTraits<Func, void>::_FuncRetType> make_inline_cb(Func&& f);
	template<typename Func>
	explicit cbchain(Func f, bool inlineflag):
		impl_(std::make_shared<cbimpl<ReturnType,void,Func>>(f))
	{
		impl_->setinline(inlineflag);
	}
};

/*
template <typename ArgType>
class cbsrc
{
	void invoke(ArgType a){
		if(impl_)
			impl_->invoke(nullptr, &a);
	}
	
	template<typename NextFunc>
	auto then(NextFunc&& f) -> typename helper::_ContinuationTypeTraits<NextFunc,ArgType>::_TaskOfType
	{
		if(impl_)
			abort();
		
		typedef typename helper::_FunctionTypeTraits<NextFunc, ArgType>::_FuncRetType _FuncRetType;
		typedef typename helper::_TaskTypeTraits<_FuncRetType>::_TaskRetType _NextCBReturnType;
		
		cbchain<_NextCBReturnType> cb;
		cb.impl_ = std::make_shared<cbimpl<_FuncRetType, ArgType, NextFunc>>( 
			std::forward<NextFunc>(f), nullptr );
		impl_ = cb.impl_;
		
		return cb;
	}
	
	std::shared_ptr<cbinterface> impl_;
};
*/


template<typename Func>
cbchain<typename helper::_FunctionTypeTraits<Func, void>::_FuncRetType> make_inline_cb(Func&& f)
{
	typedef typename helper::_FunctionTypeTraits<Func, void>::_FuncRetType _FuncRetType;
	return cbchain<_FuncRetType>(std::forward<Func>(f) , true );
}


#define cbchain_stop(Val...) \
	do{\
		cbexecinfo*p = get_current_cb(); \
		p->stop_flag_ = true; \
		return Val; \
	}while(0)

#define cbchain_retry(InputParam, RetryParam, Val...) \
	do{ \
		static_assert( std::is_same< \
			cbchain<std::decay<decltype(InputParam)>::type>, \
			std::decay<decltype(RetryParam)>::type >::value, "retry param type invalid" ); \
		cbexecinfo*p = get_current_cb(); \
		p->retry_cb_ = new std::shared_ptr<cbinterface>((RetryParam).impl_); \
		return Val; \
	}while(0)


//NEED rework
/*
#include <tuple>
template <typename Arg>
auto unpack_when_all(cbmulti*m, int& idx) -> typename helper::_TaskTypeTraits<Arg>::_TaskRetType&
{
	typedef typename helper::_TaskTypeTraits<Arg>::_TaskRetType ResArgType;
	return  *reinterpret_cast<ResArgType*>( m->getresult(idx++) );
}

template <typename Arg>
int unpack_set_multi( cbchain<Arg>&c, std::shared_ptr<cbmulti>&mcb, int& idx){
	c.impl_->setmulti(mcb, idx++);
	return 0;
}

template <typename ... T>  
void dummywrapper(T... t){}  

template <typename ... Args >
auto when_all(cbchain<Args> ... args ) -> cbchain<std::tuple<typename helper::_TaskTypeTraits<Args>::_TaskRetType&...> >
{
	int num = sizeof...(Args);
	
	typedef std::tuple<typename helper::_TaskTypeTraits<Args>::_TaskRetType&...> ReturnTupleType;
	
	std::shared_ptr<cbmulti> mcb( std::make_shared<cbmulti>(num) );
	cbmulti*m = mcb.get();

	cbchain<ReturnTupleType> cbc([m](){
		int idx=0;
		return std::make_tuple( std::ref(unpack_when_all<Args>( m, idx))... );
	});
	
	mcb->setcb(cbc.impl_);
	int idx=0;
	dummywrapper( unpack_set_multi(args,mcb,idx)... );
	return cbc;
}

template <typename STLIterator>
auto when_all(STLIterator begin, STLIterator end) -> cbchain< std::vector<typename helper::_TaskTypeTraits<typename std::iterator_traits<STLIterator>::value_type::result_type>::_TaskRetType>>
{
	typedef typename std::iterator_traits<STLIterator>::value_type::result_type ReturnType;
	typedef typename helper::_TaskTypeTraits<ReturnType>::_TaskRetType ResultType;
	
	int num=0,idx=0;
	
	for(STLIterator i=begin; i!=end; i++){
		num++;
	}
	
	std::shared_ptr<cbmulti> mcb( std::make_shared<cbmulti>(num) );
	cbmulti*m = mcb.get();
	
	cbchain<std::vector<ResultType>> cbc ([m,num](){
		std::vector<ResultType> v;
		v.resize(num);
		for(int i=0;i<num;i++){
			v[i] = *reinterpret_cast<ResultType*>( m->getresult(i) );
		}
		return std::move(v);
	});
	
	mcb->setcb(cbc.impl_);
	
	for(STLIterator i=begin; i!=end; i++){
		i->impl_->setmulti( mcb, idx++);
	}
	return cbc;
}
*/

#include <tuple>

template<typename ResultType>
struct WhenAllParam{
	WhenAllParam(int n):
		results_(n), finish_cnt_(0)
	{
	}
	
	std::vector<ResultType> results_;
	int finish_cnt_;
};


template <typename STLIterator>
auto when_all(STLIterator begin, STLIterator end) -> cbchain<std::vector<typename helper::_TaskTypeTraits<typename std::iterator_traits<STLIterator>::value_type::result_type>::_TaskRetType>>
{
	typedef typename std::iterator_traits<STLIterator>::value_type::result_type ReturnType;
	typedef typename helper::_TaskTypeTraits<ReturnType>::_TaskRetType ResultType;
	typedef std::vector<ResultType> ResultArray;
	
	int num=0,idx=0;
	
	for(STLIterator i=begin; i!=end; i++){
		num++;
	}
	
	auto wp = std::make_shared<WhenAllParam<ResultType>>(num);
	
	cbchain<ResultType> cb([](){ return ResultType();});
	
	auto mcb = cb.then([wp](ResultType&r){
		int idx = get_current_cb()->trigger_->getidx();
		
		wp->results_[idx] = std::move(r);
		wp->finish_cnt_++;
		
		if( wp->finish_cnt_ < wp->results_.size() ){
			cbchain_stop( ResultArray() );
		}
		return std::move( wp->results_ );
	});

	for(STLIterator i=begin; i!=end; i++){
		i->impl_->setidx( idx++);
		i->share_then(cb);
	}
	return mcb;
}

template <typename STLIterator>
auto when_any(STLIterator begin, STLIterator end) -> cbchain<std::tuple<int,typename helper::_TaskTypeTraits<typename std::iterator_traits<STLIterator>::value_type::result_type>::_TaskRetType&>>
{
	typedef typename std::iterator_traits<STLIterator>::value_type::result_type ReturnType;
	typedef typename helper::_TaskTypeTraits<ReturnType>::_TaskRetType ResultType;
	
	int idx=0;
	
	cbchain<ResultType> cb([](){ return ResultType();});
	
	auto mcb = cb.then([](ResultType&r){
		int idx = get_current_cb()->trigger_->getidx();
		
		return std::make_tuple(idx, std::ref(r));
	});

	for(STLIterator i=begin; i!=end; i++){
		i->impl_->setidx( idx++);
		i->share_then(cb);
	}
	return mcb;
}

}

/*

Init state:
	S1
	 |
	 |
	c1 ---> c2 ---> c3


S1 Trigger S2a

	S1     S2a
	 |      |
	 |      |
	 |      |
	c1 ---> c2 --------------> c3


S1 Trigger S2b
	S1     S2a S2b
	 |      |  |  
	 |      +--+
	 |      |
	c1 ---> c2 --------------> c3

S1 Trigger S2c
	S1     S2a S2b S2c
	 |      |  |    |
	 |      +--+----+
	 |      |
	c1 ---> c2 --------------> c3
*/


#endif 
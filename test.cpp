#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/syscall.h>

#include "utils.h"
#include "cbchain.h"
#include <utility>

using namespace callbackchain;

static int test_cb(); 

int main(int argc, char* argv[]){
	
	test_cb();
	return 0;
}


/////////////////////////////
//// cbchain test
////////////////////////////////

#include <iostream>
#include <tuple>
#include <array>
 

template<typename T>
void foo(T t){
	overloaded(t);
	overloaded( std::forward<T>(t) );
}

class Test{
public:
	Test(int v){ 
		DLOG("create"); 
		i =v;
	}
	Test(const Test&t){
		DLOG("create copy"); 
		i=t.i; 	
	}
	Test(Test&&t){
		DLOG("create move"); 
		i=t.i; 
	}
	Test& operator = (const Test&t){
		i=t.i;
		DLOG("operator copy");
	}
	~Test(){DLOG("destory");}
	void show(){ DLOG("show %d",i);}
	
	int i:31;
	unsigned int flag:1;
};


template<typename T>
T rref(T v){
	return v;
}

static int test_cb(){	
	if(0){
		cbchain<Test> s1,s2,s3;
		
		auto start_req = [&](int i){
			cbchain<Test> s;
			if(i==1){
				s1 = cbchain<Test>( [](){
					return Test(1);
				});
				s = s1;
			}
			if(i==2){
				s2 = cbchain<Test>( [](){
					return Test(2);
				});
				s = s2;
			}
			if(i==3){
				s3 = cbchain<Test>( [](){
					return Test(3);
				});
				s = s3;
			}
			return s;
		};
		
		start_req(2)
		.then([&](Test &t){
			t.show();
			return start_req(1);
		})
		.then([&](Test t){
			t.show();
			return start_req(3);
		})
		.then([&](Test t){
			t.show();
		});
		
		s2.invoke();
		s1.invoke();
		s3.invoke();
		
		s2.invoke();
		s1.invoke();
		s3.invoke();
		
	}


	if(0){
		cbchain<int> s([](){
			return 10;
		});
		cbchain<int> s1([](){
			return 333;
		});
		cbchain<int> s2([](){
			return 12;
		});
		
		s
		.then([](int v){
			DLOG("then1 %d", v);
			return v+1;
		})
		.then([s1](int v){
			DLOG("then2 %d", v);
			//cbchain_stop(cbchain<int>());
			if(v>10)
				return make_inline_cb([v](){
					DLOG("execute inline cb");
					return  v+10;
				});
			else
				return s1;
		})
		.then([&s2](int v ){
			DLOG("then3 %d", v );
			
			if(v>=10){
				int iv = 0;
				auto cb = s2
				.then([iv](int v){
					DLOG("sub1 %d", v);
					return 1+iv;
				})
				.then([](int v){
					DLOG("sub2 %d", v);
					return v+1;
				});
				
				cbchain_retry(v, rref(cb) );
			}
			
			if(v<10)
				cbchain_stop();
		});

		s.invoke();
		s1.invoke();
		
		for(int i=0; i<10; i++)
			s2.invoke();
	}
	
	
	if(1){
		
		{
			cbchain<std::unique_ptr<int>> s1([](){return std::unique_ptr<int>(new int(1));});
			cbchain<std::unique_ptr<int>> s2([](){return std::unique_ptr<int>(new int(2));});
			cbchain<std::unique_ptr<int>> s3([](){return std::unique_ptr<int>(new int(3));});
			
			std::array<cbchain<std::unique_ptr<int>>, 3> a={s1, s2, s3};
			when_all(a.begin(), a.end() )
			.then( []( std::vector<std::unique_ptr<int>>&v){
				DLOG("%d, %d, %d", 
					*v[0], *v[1], *v[2]
				);
				std::unique_ptr<int> p;
				p = std::move(v[0]);
				DLOG("p = %d", *p);
			});
			
			s1.invoke();
			s2.invoke();
			s3.invoke();
		}
		
		{
			cbchain<std::unique_ptr<int>> s1([](){return std::unique_ptr<int>(new int(10));});
			cbchain<std::unique_ptr<int>> s2([](){return std::unique_ptr<int>(new int(11));});
			cbchain<std::unique_ptr<int>> s3([](){return std::unique_ptr<int>(new int(12));});
			std::array<cbchain<std::unique_ptr<int>>, 3> a={s1, s2, s3};
			
			when_any(a.begin(), a.end() )
			.then([]( std::tuple<int, std::unique_ptr<int>&> t){
				DLOG("get %d: %d", std::get<0>(t), *(std::get<1>(t))  );
			});
			
			s1.invoke();
			s2.invoke();
			s3.invoke();
		}
	}
	
	return 0;
}









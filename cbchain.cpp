#include "cbchain.h"

namespace callbackchain{
static  cbexecinfo g_execinfo={nullptr, nullptr, false, nullptr };

void cbinterface::move(cbinterface*newcb, cbinterface*oldcb){
	//move next
	newcb->next_ =  oldcb->next_ ;

	//move idx
	newcb->multi_cb_index_ = oldcb->multi_cb_index_;
	oldcb->multi_cb_index_ = -1;
}

inline cbexecinfo* get_current_cb()
{
	return &g_execinfo;
}

void invoke_begin(cbinterface*impl,cbinterface*trigger,cbexecinfo* oldexecinfo){
	cbexecinfo*__nowexecinfo = get_current_cb();
	*oldexecinfo = *__nowexecinfo; 
	__nowexecinfo->stop_flag_ = false; 
	__nowexecinfo->retry_cb_  = nullptr; 
	__nowexecinfo->current_   = impl; 
	__nowexecinfo->trigger_   = trigger;
}

bool invoke_end(cbexecinfo* oldexecinfo){
	bool rflag=false; 
	cbexecinfo*__nowexecinfo = get_current_cb();
	
	if( __nowexecinfo->stop_flag_ ){ 
		rflag = true; 
	} 
	else if(__nowexecinfo->retry_cb_!=nullptr){ 
		std::shared_ptr<cbinterface> p(*__nowexecinfo->retry_cb_); 
		delete __nowexecinfo->retry_cb_; 
		p->setnext( __nowexecinfo->current_->ref() ); 
		if(p->isinline()) 
			p->invoke(nullptr, nullptr); 
		rflag = true; 
	} 
	*__nowexecinfo = *oldexecinfo; 
	return rflag;
}

}

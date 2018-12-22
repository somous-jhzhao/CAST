// Copyright 2008-2016 Conrad Sanderson (http://conradsanderson.id.au)
// Copyright 2008-2016 National ICT Australia (NICTA)
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ------------------------------------------------------------------------


//! \addtogroup fn_normcdf
//! @{



template<typename T1, typename T2, typename T3>
inline
typename enable_if2< (is_real<typename T1::elem_type>::value), void >::result
normcdf_helper(Mat<typename T1::elem_type>& out, const Base<typename T1::elem_type, T1>& X_expr, const Base<typename T1::elem_type, T2>& M_expr, const Base<typename T1::elem_type, T3>& S_expr)
  {
  arma_extra_debug_sigprint();
  
  #if !defined(ARMA_USE_CXX11)
    {
    arma_stop_logic_error("normcdf(): C++11 compiler required");
    
    return;
    }
  #else
    {
    typedef typename T1::elem_type eT;
    
    if(Proxy<T1>::use_at || Proxy<T2>::use_at || Proxy<T3>::use_at)
      {
      const unwrap<T1> UX(X_expr.get_ref());
      const unwrap<T2> UM(M_expr.get_ref());
      const unwrap<T3> US(S_expr.get_ref());
      
      normcdf_helper(out, UX.M, UM.M, US.M);
      
      return;
      }
    
    const Proxy<T1> PX(X_expr.get_ref());
    const Proxy<T2> PM(M_expr.get_ref());
    const Proxy<T3> PS(S_expr.get_ref());
    
    arma_debug_check( ( (PX.get_n_rows() != PM.get_n_rows()) || (PX.get_n_cols() != PM.get_n_cols()) || (PM.get_n_rows() != PS.get_n_rows()) || (PM.get_n_cols() != PS.get_n_cols()) ), "normcdf(): size mismatch" );
    
    out.set_size(PX.get_n_rows(), PX.get_n_cols());
    
    eT* out_mem = out.memptr();
    
    const uword N = PX.get_n_elem();
    
    typename Proxy<T1>::ea_type X_ea = PX.get_ea();
    typename Proxy<T2>::ea_type M_ea = PM.get_ea();
    typename Proxy<T3>::ea_type S_ea = PS.get_ea();
    
    const bool use_mp = arma_config::cxx11 && arma_config::openmp && mp_gate<eT,true>::eval(N);
    
    if(use_mp)
      {
      #if defined(ARMA_USE_OPENMP)
        {
        const int n_threads = mp_thread_limit::get();
        #pragma omp parallel for schedule(static) num_threads(n_threads)
        for(uword i=0; i<N; ++i)
          {
          const eT tmp = (X_ea[i] - M_ea[i]) / (S_ea[i] * (-Datum<eT>::sqrt2));
          
          out_mem[i] = 0.5 * std::erfc(tmp);
          }
        }
      #endif
      }
    else
      {
      for(uword i=0; i<N; ++i)
        {
        const eT tmp = (X_ea[i] - M_ea[i]) / (S_ea[i] * (-Datum<eT>::sqrt2));
        
        out_mem[i] = 0.5 * std::erfc(tmp);
        }
      }
    }
  #endif
  }



template<typename eT>
arma_inline
typename enable_if2< (is_real<eT>::value), eT >::result
normcdf(const eT x)
  {
  #if !defined(ARMA_USE_CXX11)
    {
    arma_stop_logic_error("normcdf(): C++11 compiler required");
    
    return eT(0);
    }
  #else
    {
    const eT out = 0.5 * std::erfc( x / (-Datum<eT>::sqrt2) );
    
    return out;
    }
  #endif
  }



template<typename eT>
inline
typename enable_if2< (is_real<eT>::value), eT >::result
normcdf(const eT x, const eT mu, const eT sigma)
  {
  #if !defined(ARMA_USE_CXX11)
    {
    arma_stop_logic_error("normcdf(): C++11 compiler required");
    
    return eT(0);
    }
  #else
    {
    const eT tmp = (x - mu) / (sigma * (-Datum<eT>::sqrt2));
    
    const eT out = 0.5 * std::erfc(tmp);
    
    return out;
    }
  #endif
  }



template<typename eT, typename T2, typename T3>
inline
typename enable_if2< (is_real<eT>::value), Mat<eT> >::result
normcdf(const eT x, const Base<eT, T2>& M_expr, const Base<eT, T3>& S_expr)
  {
  arma_extra_debug_sigprint();
  
  const quasi_unwrap<T2> UM(M_expr.get_ref());
  const Mat<eT>&     M = UM.M;
  
  Mat<eT> out;
  
  normcdf_helper(out, x*ones< Mat<eT> >(size(M)), M, S_expr.get_ref());
  
  return out;
  }



template<typename T1>
inline
typename enable_if2< (is_real<typename T1::elem_type>::value), Mat<typename T1::elem_type> >::result
normcdf(const Base<typename T1::elem_type, T1>& X_expr)
  {
  arma_extra_debug_sigprint();
  
  typedef typename T1::elem_type eT;
  
  const quasi_unwrap<T1> UX(X_expr.get_ref());
  const Mat<eT>&     X = UX.M;
  
  Mat<eT> out;
  
  normcdf_helper(out, X, zeros< Mat<eT> >(size(X)), ones< Mat<eT> >(size(X)));
  
  return out;
  }



template<typename T1>
inline
typename enable_if2< (is_real<typename T1::elem_type>::value), Mat<typename T1::elem_type> >::result
normcdf(const Base<typename T1::elem_type, T1>& X_expr, const typename T1::elem_type mu, const typename T1::elem_type sigma)
  {
  arma_extra_debug_sigprint();
  
  typedef typename T1::elem_type eT;
  
  const quasi_unwrap<T1> UX(X_expr.get_ref());
  const Mat<eT>&     X = UX.M;
  
  Mat<eT> out;
  
  normcdf_helper(out, X, mu*ones< Mat<eT> >(size(X)), sigma*ones< Mat<eT> >(size(X)));
  
  return out;
  }



template<typename T1, typename T2, typename T3>
inline
typename enable_if2< (is_real<typename T1::elem_type>::value), Mat<typename T1::elem_type> >::result
normcdf(const Base<typename T1::elem_type, T1>& X_expr, const Base<typename T1::elem_type, T2>& M_expr, const Base<typename T1::elem_type, T3>& S_expr)
  {
  arma_extra_debug_sigprint();
  
  typedef typename T1::elem_type eT;
  
  Mat<eT> out;
  
  normcdf_helper(out, X_expr.get_ref(), M_expr.get_ref(), S_expr.get_ref());
  
  return out;
  }



//! @}

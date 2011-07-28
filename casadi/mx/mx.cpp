/*
 *    This file is part of CasADi.
 *
 *    CasADi -- A symbolic framework for dynamic optimization.
 *    Copyright (C) 2010 by Joel Andersson, Moritz Diehl, K.U.Leuven. All rights reserved.
 *
 *    CasADi is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    CasADi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with CasADi; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "mx.hpp"
#include "mx_node.hpp"
#include "mx_tools.hpp"
#include "unary_op.hpp"
#include "binary_op.hpp"
#include "mapping.hpp"
#include "../fx/sx_function.hpp"
#include "evaluation.hpp"
#include "symbolic_mx_node.hpp"
#include "mx_constant.hpp"
#include "mx_tools.hpp"
#include "../stl_vector_tools.hpp"
#include "../matrix/matrix_tools.hpp"
#include "multiplication.hpp"

using namespace std;
namespace CasADi{

MX::~MX(){
}

MX::MX(){
}

MX::MX(double x){
  assignNode(new MXConstant(x));
}

MX::MX(const Matrix<double> &x){
  assignNode(new MXConstant(x));
}

MX::MX(const vector<double> &x){
  assignNode(new MXConstant(x));
}

MX::MX(const string& name, int n, int m){
  assignNode(new SymbolicMatrix(name,n,m));
}

MX::MX(const std::string& name,const std::pair<int,int> &nm) {
  assignNode(new SymbolicMatrix(name,nm.first,nm.second));
}

MX::MX(const string& name, const CRSSparsity & sp){
  assignNode(new SymbolicMatrix(name,sp));
}

MX::MX(int nrow, int ncol){
  assignNode(new Mapping(CRSSparsity(nrow,ncol)));
}

MX::MX(const CRSSparsity& sp, const MX& val){
  // Make sure that val is dense and scalar
  casadi_assert(val.numel()==1);
  
  // Dense matrix if val dense
  if(val.dense()){
    // Create mapping
    assignNode(new Mapping(sp));
    (*this)->addDependency(val,vector<int>(sp.size(),0));
    simplifyMapping(*this);
  } else {
    // Empty matrix
    *this = zeros(sp.size1(),sp.size2());
  }
}

MX MX::create(MXNode* node){
  MX ret;
  ret.assignNode(node);
  return ret;
}

const MX MX::getSub(int i, const std::vector<int>& j) const{
  return getSub(vector<int>(1,i),j);
}

const MX MX::getSub(const std::vector<int>& i, int j) const{
  return getSub(i,vector<int>(1,j));
}

const MX MX::getSub(const vector<int>& ii, const vector<int>& jj) const{
  // Nonzero mapping from submatrix to full
  vector<int> mapping;
  
  // Get the sparsity pattern
  CRSSparsity sp = sparsity().getSub(ii,jj,mapping);
 
  // Create return MX
  MX ret = MX::create(new Mapping(sp));
  ret->addDependency(*this,mapping);
  simplifyMapping(ret);
  return ret;
}

const MX MX::getSub(int i, int j) const{
  int ind = sparsity().getNZ(i,j);

  MX ret;
  if (ind>=0) {
    const CRSSparsity& sp = CRSSparsity::scalarSparsity;
    ret.assignNode(new Mapping(sp));
    ret->addDependency(*this,vector<int>(1,ind));
  } else {
    const CRSSparsity& sp = CRSSparsity::scalarSparsitySparse;
    ret.assignNode(new Mapping(sp));
    ret->addDependency(*this,vector<int>(0));
  }
//  simplifyMapping(ret);
  return ret;
}

void MX::setSub(int i, int j, const MX& el){
  setSub(vector<int>(1,i),vector<int>(1,j),el);
}

void MX::setSub(int i, const std::vector<int>& j, const MX& el){
  setSub(vector<int>(1,i),j,el);
}

void MX::setSub(const std::vector<int>& i, int j, const MX& el){
  setSub(i,vector<int>(1,j),el);
}

void MX::setSub(const vector<int>& ii, const vector<int>& jj, const MX& el){
  // Allow el to be a 1x1
  if (el.size()==1 && el.numel()==1) {
    if (ii.size()>1 || jj.size()>1) {
      setSub(ii,jj,MX(ii.size(),jj.size(),el));
      return;
    }
  }
  casadi_assert_message(ii.size()==el.size1(),"right hand size must match dimension of left hand side in assignment");
  casadi_assert_message(jj.size()==el.size2(),"right hand size must match dimension of left hand side in assignment");
  if(dense() && el.dense()){
    // Dense mode
    for(int i=0; i<ii.size(); ++i) {
      for(int j=0; j<jj.size(); ++j) {
        (*this)[ii[i]*size2() + jj[j]]=el[i*el.size2()+j];
      }
    }
  } else {
    // Sparse mode

    // Remove submatrix to be replaced
    erase(ii,jj);

    // Extend el to the same dimension as this
    MX el_ext = el;
    el_ext.enlarge(size1(),size2(),ii,jj);

    // Unite the sparsity patterns
    *this = unite(*this,el_ext);
  }
}

MX MX::getNZ(int k) const{
  if (k<0) k+=size();
  if (k>=size()) {
    stringstream ss;
    ss << "MX::getNZ: requested at(" <<  k << "), but that is out of bounds:  " << dimString() << ".";
    throw CasadiException(ss.str());
  }
  return getNZ(vector<int>(1,k));
}

MX MX::getNZ(const vector<int>& k) const{
  CRSSparsity sp(k.size(),1,true);
  MX ret;
  ret.assignNode(new Mapping(sp));
  ret->addDependency(*this,k);
  //simplifyMapping(ret);
  return ret;
}

void MX::setNZ(int k, const MX& el){
  if (k<0) k+=size();
  if (k>=size()) {
    stringstream ss;
    ss << "MX::setNZ: requested at(" <<  k << "), but that is out of bounds:  " << dimString() << ".";
    throw CasadiException(ss.str());
  }
  setNZ(vector<int>(1,k),el);
}

void MX::setNZ(const vector<int>& k, const MX& el){
  if (k.size()!=el.size() && el.size()!=1) {
    std::stringstream ss;
    ss << "MX::setNZ: length of non-zero indices (" << k.size() << ") " << std::endl;
    ss << "must match size of rhs (" << el.size() << ")." << std::endl;
    throw CasadiException(ss.str());
  }
  MX ret;
  ret.assignNode(new Mapping(sparsity()));
  ret->addDependency(*this,range(size()));
  if (el.size()==1) {
    ret->addDependency(el,std::vector<int>(k.size(),0),k);
  } else {
    ret->addDependency(el,range(k.size()),k);
  }
  simplifyMapping(ret);
  *this = ret;
}

const MX MX::at(int k) const {
  return getNZ(k); 
}

/// Access a non-zero element
NonZeros<MX,int> MX::at(int k) {
  return NonZeros<MX,int>(*this,k);
}

int MX::size() const{
  return sparsity().size();
}

int MX::size1() const{
  return sparsity().size1();
}

int MX::numel() const{
  return sparsity().numel();
}

int MX::size2() const{
  return sparsity().size2();
}

MX operator+(const MX &x, const MX &y){
  bool samedim = x.size1()==y.size1() && x.size2()==y.size2();
  if((samedim || x.numel()==1) && isZero(x)){
    return y;
  } else if((samedim || y.numel()==1) && isZero(y)){
    return x;
  } else if(y->isOperation(NEG)){
    return x - y->dep(0);
  } else if(x->isOperation(NEG)){
    return y - x->dep(0);
  } else {
    return MX::binary(ADD,x,y);
  }
}

MX operator-(const MX &x, const MX &y){
  bool samedim = x.size1()==y.size1() && x.size2()==y.size2();
  if((samedim || x.numel()==1) && isZero(x)){
    return -y;
  } else if((samedim || y.numel()==1) && isZero(y)){
    return x;
  } else if(y->isOperation(NEG)){
    return x+y->dep(0);
  } else if(y.get()==x.get()){
    return MX::zeros(x.size1(),x.size2());
  } else {
    return MX::binary(SUB,x,y);
  }
}

MX MX::binary(int op, const MX &x, const MX &y){
  // Make sure that dimensions match
  casadi_assert_message((x.numel()==1 || y.numel()==1 || (x.size1()==y.size1() && x.size2()==y.size2())),"Dimension mismatch");
  
  // Quick return if zero
  if((casadi_math<double>::f0x_is_zero[op] && isZero(x)) || 
    (casadi_math<double>::fx0_is_zero[op] && isZero(y))){
    return zeros(std::max(x.size1(),y.size1()),std::max(x.size2(),y.size2()));
  }
  
  // Create binary node
  if(x.numel()==1)
    return scalar_matrix(op,x,y);
  else if(y.numel()==1)  
    return matrix_scalar(op,x,y);
  else
    return matrix_matrix(op,x,y);
}

MX MX::unary(int op, const MX &x){
  // Quick return if zero
  if(casadi_math<double>::f0x_is_zero[op] && isZero(x)){
    return zeros(x.size1(),x.size2());
  } else {
    return create(new UnaryOp(Operation(op),x));
  }
}

MX MX::scalar_matrix(int op, const MX &x, const MX &y){
  return matrix_matrix(op,x,y);
}

MX MX::matrix_scalar(int op, const MX &x, const MX &y){
  return matrix_matrix(op,x,y);
}

MX MX::matrix_matrix(int op, const MX &x, const MX &y){
  return create(new SparseSparseOp(Operation(op),x,y)); 
}

MX operator*(const MX &x, const MX &y){
  bool samedim = x.size1()==y.size1() && x.size2()==y.size2();
  if((samedim || x.numel()==1) && isOne(x)){
    return y;
  } else if((samedim || x.numel()==1) && isMinusOne(x)){
    return -y;
  } else if((samedim || y.numel()==1) && isOne(y)){
    return x;
  } else if((samedim || y.numel()==1) && isMinusOne(y)){
    return -x;
  } else {
    return MX::binary(MUL,x,y);
  }
}

MX operator/(const MX &x, const MX &y){
  bool samedim = x.size1()==y.size1() && x.size2()==y.size2();
  if((samedim || y.numel()==1) && isOne(y)){
    return x;
  } else {
    return MX::binary(DIV,x,y);
  }
}

MXNode* MX::operator->(){
  return (MXNode*)SharedObject::operator->();
}

const MXNode* MX::operator->() const{
  return (const MXNode*)SharedObject::operator->();
}

MX& MX::operator+=(const MX &y){
  return *this = *this + y;
}

MX& MX::operator-=(const MX &y){
  return *this = *this - y;
}

MX& MX::operator*=(const MX &y){
  return *this = *this * y;
}

MX& MX::operator/=(const MX &y){
  return *this = *this / y;
}

bool MX::empty() const{
  return numel()==0;
}

bool MX::dense() const{
  return numel()==size();
}

MX MX::zeros(int nrow, int ncol){
  return MX(nrow,ncol);
}

MX MX::zeros(const CRSSparsity& sparsity){
  return DMatrix(sparsity,0);
}

MX MX::ones(int nrow, int ncol){
  return MX(Matrix<double>(nrow,ncol,1));
}

MX MX::eye(int n){
  Matrix<double> I(CRSSparsity::createDiagonal(n),1);
  return MX(I);
}


MX MX::operator-() const{
  if((*this)->isOperation(NEG)){
    return (*this)->dep(0);
  } else {
    return unary(NEG,*this);
  }
}

MX::MX(const MX& x) : SharedObject(x){
}

const CRSSparsity& MX::sparsity() const{
  return (*this)->sparsity();
}

CRSSparsity& MX::sparsityRef(){
  // Since we can potentially change the behavior of the MX node, we must make a deep copy if there are other references
  makeUnique();
  
  // Return the reference, again, deep copy if multiple references
  (*this)->sparsity_.makeUnique();
  return (*this)->sparsity_;
}

void MX::erase(const vector<int>& ii, const vector<int>& jj){
  // Get sparsity of the new matrix
  CRSSparsity sp = sparsity();
  
  // Erase from sparsity pattern
  vector<int> mapping = sp.erase(ii,jj);
  
  // Create new matrix
  if(mapping.size()!=size()){
    MX ret;
    ret.assignNode(new Mapping(sp));
    ret->addDependency(*this,mapping);
    simplifyMapping(ret);
    *this = ret;
  }
}

void MX::enlarge(int nrow, int ncol, const vector<int>& ii, const vector<int>& jj){
  CRSSparsity sp = sparsity();
  sp.enlarge(nrow,ncol,ii,jj);
  
  MX ret;
  ret.assignNode(new Mapping(sp));
  ret->addDependency(*this,range(size()));
  simplifyMapping(ret);

  *this = ret;
}

MX::MX(int nrow, int ncol, const MX& val){
  // Make sure that val is scalar
  casadi_assert(val.numel()==1);
  casadi_assert(val.size()==1);
  
  CRSSparsity sp(nrow,ncol,true);
  assignNode(new Mapping(sp));
  (*this)->addDependency(val,vector<int>(sp.size(),0));
}

Matrix<double> MX::eval(const vector<Matrix<double> >& x){
  casadi_assert_message((*this)->ndep()==x.size(),"incorrect number of arguments");
  return (*this)->eval(x);
}

Matrix<SX> MX::eval(const vector<Matrix<SX> >& x){
  casadi_assert_message((*this)->ndep()==x.size(),"incorrect number of arguments");
  return (*this)->eval(x);
}

MX MX::eval(const vector<MX>& x){
  casadi_assert_message((*this)->ndep()==x.size(),"incorrect number of arguments");
  return (*this)->eval(x);
}

std::string MX::dimString() const {
  std::stringstream ss;
  ss << "(" << size1() << "x" << size2() << "=" << numel() << "|" << size() << ")";
  return ss.str();
}

MX MX::jac(int iind){
  return (*this)->jac(iind);
}

const Matrix<int>& MX::mapping() {
  const Mapping * m = dynamic_cast<const Mapping*>(get());
  casadi_assert_message(m!=0, "mapping: argument MX should point to a Mapping node");
  casadi_assert_message(m->ndep()<=1, "mapping: argument MX should be a Mapping with one depency only (or zero dependencies)");
  return m->nzmap_;
}

MX MX::prod(const MX& y) const{
  const MX& x = *this;

  // Check if we can simplify the product
  if(isIdentity(x)){
    return y;
  } else if(isIdentity(y)){
    return x;
  } else if(isZero(x) || isZero(y)){
    return MX::zeros(x.size1(),y.size2());
  } else if(x.numel()==1 || y.numel()==1){
    return x*y;
  } else if(x.sparsity().diagonal() && y.size2()==1){
    return diag(x)*y;
  } else if(y.sparsity().diagonal() && x.size1()==1){
    return x*trans(diag(y));
  } else {
    return MX::create(new Multiplication(x,trans(y)));
  }
}

MX MX::rprod(const MX& y) const{
  return y.prod(*this);
}

MX MX::inner_prod(const MX& y) const{
  const MX& x = *this;
  casadi_assert_message(x.size2()==1,"inner_prod: first factor not a vector");
  casadi_assert_message(y.size2()==1, "inner_prod: second factor not a vector");
  casadi_assert_message(x.size1()==y.size1(),"inner_prod: dimension mismatch");
  MX sum = 0;
  for(int i=0; i<x.size1(); ++i)
    sum += x(i)*y(i);
  return sum;
}

MX MX::outer_prod(const MX& y) const{
  return prod(trans(y));
}

MX MX::__pow__(const MX& n) const{
  if(n->isConstant()){
    return MX::binary(CONSTPOW,*this,n);
  } else {
    return MX::binary(POW,*this,n);
  }
}

MX MX::constpow(const MX& b) const{    
  return binary(CONSTPOW,*this,b);
}

MX MX::fmin(const MX& b) const{       
  return binary(FMIN,*this,b);
}

MX MX::fmax(const MX& b) const{       
  return binary(FMAX,*this,b);
}

MX MX::exp() const{ 
  return unary(EXP,*this);
}

MX MX::log() const{ 
  return unary(LOG,*this);
}

MX MX::sqrt() const{ 
  return unary(SQRT,*this);
}

MX MX::sin() const{ 
  return unary(SIN,*this);
}

MX MX::cos() const{ 
  return unary(COS,*this);
}

MX MX::tan() const{ 
  return unary(TAN,*this);
}

MX MX::arcsin() const{ 
  return unary(ASIN,*this);
}

MX MX::arccos() const{ 
  return unary(ACOS,*this);
}

MX MX::arctan() const{ 
  return unary(ATAN,*this);
}

MX MX::floor() const{ 
  return unary(FLOOR,*this);
}

MX MX::ceil() const{ 
  return unary(CEIL,*this);
}

MX MX::erf() const{ 
  return unary(ERF,*this);
}

MX MX::__add__(const MX& b) const{    return *this + b;}
MX MX::__radd__(const MX& b) const{   return b + *this;}
MX MX::__sub__(const MX& b) const{    return *this - b;}
MX MX::__rsub__(const MX& b) const{   return b - *this;}
MX MX::__mul__(const MX& b) const{    return *this * b;}
MX MX::__rmul__(const MX& b) const{   return b * *this;}
MX MX::__div__(const MX& b) const{    return *this / b;}
MX MX::__rdiv__(const MX& b) const{   return b / *this;}
MX MX::__rpow__(const MX& b) const {   return std::pow(b,*this);}
MX MX::__mrdivide__  (const MX& b) const { if (MX(b).numel()==1) return *this/b; throw CasadiException("mrdivide: Not implemented");}
MX MX::__rmrdivide__ (const MX& b) const { if ((*this).numel()==1) return b/(*this); throw CasadiException("rmrdivide: Not implemented");}
MX MX::__ldivide__   (const MX& b) const { if (MX(b).numel()==1) return *this/b; throw CasadiException("mldivide: Not implemented");}
MX MX::__rmldivide__ (const MX& b) const { if ((*this).numel()==1) return b/(*this); throw CasadiException("rmldivide: Not implemented");}
MX MX::__mpower__(const MX& b) const  {   return std::pow(*this,b); throw CasadiException("mpower: Not implemented");}
MX MX::__rmpower__(const MX& b) const {   return std::pow(b,*this); throw CasadiException("rmpower: Not implemented");}

} // namespace CasADi

using namespace CasADi;
namespace std{

MX exp(const MX& x){
  return x.exp();
}

MX log(const MX& x){
  return x.log();
}

MX sqrt(const MX& x){
  return x.sqrt();
}

MX sin(const MX& x){
  return x.sin();
}

MX cos(const MX& x){
  return x.cos();
}

MX tan(const MX& x){
  return x.tan();
}

MX atan(const MX& x){
  return x.arctan();
}

MX asin(const MX& x){
  return x.arcsin();
}

MX acos(const MX& x){
  return x.arccos();
}

MX pow(const MX& x, const MX& n){
  return x.__pow__(n);
}

MX constpow(const MX& x, const MX& n){
  return x.constpow(n);
}


MX floor(const MX& x){
  return x.floor();
}

MX ceil(const MX& x){
  return x.ceil();
}

} // namespace std

// GLobal namespace
MX erf(const MX& x){
  return x.erf();
}

MX fmin(const MX& x, const MX& y){
  return x.fmin(y);
}

MX fmax(const MX& x, const MX& y){
  return x.fmax(y);
}

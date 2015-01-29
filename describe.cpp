//#include "headers.hpp"
//#include "strtk.hpp"
#include <iostream>
#include <iterator>
#include <vector>
#include <deque>
#include <sstream>
#include <string>
#include <cmath>
#include <cfloat>
#include <stdexcept>
#include <algorithm>
#include <numeric>
#include "spline.h"

using namespace std;

template<typename T>
auto read_header(T& in) {
  string header; getline(in,header);
  vector<string> col_names;
  istringstream iss_header(header);
  string tmp_tok; getline(iss_header,tmp_tok,',');
  for(string tok; getline(iss_header,tok,',');col_names.push_back(tok));
  return col_names;
}

int main()
{
  cin.sync_with_stdio(false);
  auto col_names = read_header(cin);

  //// by default, display (based on pandas describe) count, mean, std, min, 25, 50, 75th precentile, and max for each column excluding NaN. output number of nans and missing values.
  vector<size_t> missing(col_names.size(),0);
  vector<size_t> infs(col_names.size(),0);
  vector<size_t> nans(col_names.size(),0);
  vector<size_t> count(col_names.size(),0);
  vector<vector<double>> xv(col_names.size());
  vector<double> mean(col_names.size(),0);
  vector<double> std(col_names.size(),0);
  vector<double> var(col_names.size(),0);
  vector<double> ske(col_names.size(),0);
  vector<double> kur(col_names.size(),0);
  vector<double> sum(col_names.size(),0);
  vector<double> sum2(col_names.size(),0);
  vector<double> sum3(col_names.size(),0);
  vector<double> sum4(col_names.size(),0);
  vector<double> min(col_names.size());
  vector<double> max(col_names.size());
  //const double p=0.5;
  const size_t num_quantiles =19;
  const size_t num_markers = num_quantiles*2+3;
  vector<vector<double>> heights(col_names.size(),vector<double>(num_markers,0));              // q_i
  vector<vector<double>> actual_positions(col_names.size(),vector<double>(num_markers,0));     // n_i
  vector<vector<double>> desired_positions(col_names.size(),vector<double>(num_markers,0));    // n'_i
  vector<vector<double>> positions_increments(col_names.size(),vector<double>(num_markers,0)); // dn'_i
  vector<double> probabilities(num_quantiles);
  for(size_t i=0; i<num_quantiles;++i){
    probabilities[i]=(i+1.0)/ (num_quantiles+1);
  }
  for(size_t icol=0; icol< col_names.size();++icol){
    for(std::size_t i = 0; i < num_markers; ++i) {
      actual_positions[icol][i] = i + 1;
    }
    positions_increments[icol][0] = 0.;
    positions_increments[icol][num_markers - 1] = 1.;
    for(std::size_t i = 0; i < num_quantiles; ++i) {
      positions_increments[icol][2 * i + 2] = probabilities[i];
    }
    for(std::size_t i = 0; i <= num_quantiles; ++i) {
      positions_increments[icol][2 * i + 1] = 0.5 * (positions_increments[icol][2 * i] + positions_increments[icol][2 * i + 2]);
    }
    for(std::size_t i = 0; i < num_markers; ++i) {
      desired_positions[icol][i] = 1. + 2. * (num_quantiles + 1.) * positions_increments[icol][i];
    }
  }
  const size_t bins = 2;///> number of bins
  const size_t bi = 2*bins;///> initial number of bins
  const size_t bl = 2*bi;///> limiting  number of bins
  const size_t buffer_size = bi*1;
  double bin_size=0.0;
  vector<vector<double>> buffer(col_names.size()); ///> number of elements to buffer
  for(auto &i:buffer) i.reserve(buffer_size);
  vector<deque<double>> binloc(col_names.size(), deque<double>(bi));///> working histogram
  vector<deque<size_t>> whist(col_names.size(), deque<size_t>(bi,0));///> working histogram
  vector<vector<double>> hist(col_names.size(), vector<double>(bins,0));///> final histogram
  vector<vector<double>> histloc(col_names.size(), vector<double>(bins,0));///> final histogram
  bool first_line_not_read =1;
  for(string line;getline(cin,line);){
    istringstream issl(line);
    string idx; getline(issl,idx,',');///> skip the first field since it's id
    size_t icol =0;
    for(string tok;getline(issl,tok,','); ){
      double x;
      if(!tok.empty()){
        try {
          x = stod(tok);
        } catch (const std::invalid_argument&) {
          cerr << "Error: stod convertion of the cell is invalid.\n";
          throw;
        } catch (const std::out_of_range&) {
          cerr << "Error: stod convertion of the cell is out of range.\n";
          throw;
        }
        if(std::isfinite(x)){
          if(first_line_not_read){
            min[icol]=x;
            max[icol]=x;
            first_line_not_read=0;
          }else{
            if(x<min[icol]) min[icol]=x;
            if(x>max[icol]) max[icol]=x;
          }
          count[icol]++;
          sum[icol]+=x;
          double x2=x*x;
          sum2[icol]+=x2;
          sum3[icol]+=x2*x;
          sum4[icol]+=x2*x2;
          //xv[icol].push_back(x);///> intropolate instead of p2
          ///
          /// p2
          ///
          std::size_t cnt = count[icol];
          // first accumulate num_markers samples
          if (cnt<buffer_size+1){///> count starts from 0, therefore +1
            buffer[icol].push_back(x); ///> fill buffer
          }
          if(cnt==buffer_size){ ///> full buffer --> build bins
            /// p2 quantiles::
            //size_t iqp = 0;
            for(size_t im=1; im<probabilities.size()-1;++im){
              auto iq = static_cast<size_t>(probabilities[im]*cnt);
              std::nth_element(begin(buffer[icol]),begin(buffer[icol])+iq,end(buffer[icol]));
              actual_positions[icol][im]=iq;
              desired_positions[icol][im]=iq;
              heights[icol][im]= buffer[icol][iq];
              //heights[icol][im]= std::accumulate(begin(buffer[icol])+iqp,begin(buffer[icol])+iq,heights[icol][im-1]);
              //iqp=iq;
            }
            /// hist:
            auto mmp = std::minmax_element(begin(buffer[icol]),end(buffer[icol]));
            bin_size = (*mmp.second-*mmp.first)/(bi);
            for(size_t i=0; i<bi; ++i) {///> create bins
              binloc[icol][i]=*mmp.first + (i+1)*bin_size;///> ends of the bins are stored
            }
            for(size_t ibuf=0; ibuf<buffer_size; ++ibuf) {///> fill bins
              auto it = std::lower_bound( binloc[icol].begin() , binloc[icol].end() , buffer[icol][ibuf]);
              ++whist[icol][std::distance(binloc[icol].begin(), it)];
            }
            //vector<double>().swap(buffer[icol]);///> clear space for the buffer
          //if(cnt <= num_markers) {
            //heights[icol][cnt - 1] = x;
            //// complete the initialization of heights[icol] by sorting
            //if(cnt == num_markers) {
              //std::sort(heights[icol].begin(), heights[icol].end());
            //}
          } else {
            std::size_t sample_cell = 1;
            // find cell k = sample_cell such that heights[icol][k-1] <= sample < heights[icol][k]
            if(x < heights[icol][0]) {
              heights[icol][0] = x;
              sample_cell = 1;
            } else if(x >= heights[icol][num_markers - 1]) {
              heights[icol][num_markers - 1] = x;
              sample_cell = num_markers - 1;
            } else {
              auto it = std::upper_bound( heights[icol].begin() , heights[icol].end() , x);
              sample_cell = std::distance(heights[icol].begin(), it);
            }
            // update actual positions of all markers above sample_cell index
            for(std::size_t i = sample_cell; i < num_markers; ++i) {
              ++actual_positions[icol][i];
            }
            // update desired positions of all markers
            for(std::size_t i = 0; i < num_markers; ++i) {
              desired_positions[icol][i] += positions_increments[icol][i];
            }
            // adjust heights[icol] and actual positions of markers 1 to num_markers-2 if necessary
            for(std::size_t i = 1; i <= num_markers - 2; ++i) {
              // offset to desired position
              double d = desired_positions[icol][i] - actual_positions[icol][i];
              // offset to next position
              double dp = actual_positions[icol][i+1] - actual_positions[icol][i];
              // offset to previous position
              double dm = actual_positions[icol][i-1] - actual_positions[icol][i];
              // height ds
              if((d >= 1 && dp > 1) || (d <= -1 && dm < -1)) {
                double hp = (heights[icol][i+1] - heights[icol][i]) / dp;
                double hm = (heights[icol][i-1] - heights[icol][i]) / dm;
                bool neg_d = signbit(d);
                // try adjusting heights[icol][i] using p-squared formula
                double h;
                if(neg_d) {
                  h = heights[icol][i] -((-1-dm)*hp +(dp+1)*hm)/(dp-dm);
                }else{
                  h = heights[icol][i] +(( 1-dm)*hp +(dp-1)*hm)/(dp-dm);
                }
                if(heights[icol][i - 1] < h && h < heights[icol][i + 1]) {
                    heights[icol][i] = h;
                } else {
                    // use linear formula
                    if(d > 0) {
                        heights[icol][i] += hp;
                    }
                    if(d < 0) {
                        heights[icol][i] -= hm;
                    }
                }
                if(neg_d) {
                  actual_positions[icol][i] --;
                }else{
                  actual_positions[icol][i] ++;
                }
              }
            }
          }
          ///
          /// p2 end
          ///
          ///
          /// hist
          ///
          //Algorithm:
          //      combine each two bins into one.
          //  Note: to do spline use much 3x more bins
          if (cnt<buffer_size+1){///> fills up initial histogram by using buffer or initial elements (count starts from 0, therefore +1)
            buffer[icol].push_back(x); ///> fill buffer
            if(cnt==buffer_size){ ///> full buffer --> build bins
              auto mmp = std::minmax_element(begin(buffer[icol]),end(buffer[icol]));
              bin_size = (*mmp.second-*mmp.first)/(bi);
              for(size_t i=0; i<bi; ++i) {///> create bins
                binloc[icol][i]=*mmp.first + (i+1)*bin_size;///> ends of the bins are stored
              }
              for(size_t ibuf=0; ibuf<buffer_size; ++ibuf) {///> fill bins
                auto it = std::lower_bound( binloc[icol].begin() , binloc[icol].end() , buffer[icol][ibuf]);
                ++whist[icol][std::distance(binloc[icol].begin(), it)];
              }
            }
          }else{ ///>  add a new element or another buffer of elements
            if(x<=binloc[icol].front()-bin_size){ ///>    add new bins of the same size in the beginning or the end so that a new element fits in
              while(x<=binloc[icol].front()-bin_size){
                binloc[icol].push_front(binloc[icol].front()-bin_size);
                whist[icol].push_front(0);
              }
              whist[icol].front()=1;
            }else if(binloc[icol].back()<x) {
              while(binloc[icol].back()<x) {
                binloc[icol].push_back(binloc[icol].back()+bin_size);
                whist[icol].push_back(0);
              }
              whist[icol].back()=1;
            }else{
              auto it = std::lower_bound( binloc[icol].begin() , binloc[icol].end() , x);
              ++whist[icol][std::distance(binloc[icol].begin(), it)];
            }
            while(binloc[icol].size()>=bl){///> combine bins when number of bins doubled
              deque<double> tmp_binloc;
              deque<size_t> tmp_whist;
              for(size_t i=0; i+1< binloc[icol].size();++(++i)){
                tmp_binloc.push_back(binloc[icol][i+1]);
                tmp_whist.push_back(whist[icol][i]+whist[icol][i+1]);
              }
              if(tmp_binloc.size()*2==binloc[icol].size()-1){
                tmp_binloc.push_back(binloc[icol].back()+bin_size);
                tmp_whist.push_back(whist[icol].back());
              }else if(tmp_binloc.size()*2==binloc[icol].size()){
              }else{
                cout << "error: sizes of bins during combining don't match\n";
              }
              swap(tmp_binloc,binloc[icol]);
              swap(tmp_whist,whist[icol]);
              bin_size*=2;
            }
          }
          ///
          /// hist end
          ///
        }
        else if(std::isinf(x))
          infs[icol]++;
        else if(std::isnan(x))
          nans[icol]++;
        else
          cerr << "error: caticorization of input cell didn't work.\n";
      }else {
        missing[icol]++;
      }
      ++icol;
    }
  }
  for(size_t i=0; i<missing.size();++i){
    double n = count[i];
    double k = sum[i]/n,k2=k*k,k3=k2*k,k4=k2*k2;
    mean[i] = k;
    double sum1s = sum[i]-n*k;
    double sum2s = sum2[i] -2.0*k*sum[i] +n*k2;
    double sum3s = sum3[i] -3*k*sum2[i] +3*k2*sum[i] -n*k3;
    double sum4s = sum4[i] -4*k*sum3[i] +6*k2*sum2[i] -4*k3*sum[i] +n*k4;
    double m1 = sum1s/n, m12=m1*m1, m13=m12*m1,m14=m12*m12;
    double m2 = sum2s/n;
    double m3 = sum3s/n;
    double m4 = sum4s/n;
    double variance = m2-m12;
    //double sample_variance = variance*n/(n-1);
    var[i] = variance;
    double stdev = sqrt(variance);
    //double sample_stdev = sqrt((sum2s - sum1s*k)/(n-1));
    std[i] = stdev;
    double skewness = (m3 -3*m1*m2 +2*m13)/(variance*stdev);
    //double skew_bias = sqrt(pow((n-1)/n,3));
    //double sample_skewness = skewness*skew_bias;
    ske[i] = skewness;
    double kurtosis = (m4 -4*m1*m3 +6*m12*m2 -3*m14)/(variance*variance)-3;
    kur[i] = kurtosis;
    ///
    /// qhist
    ///
    vector<vector<double>> qhist(col_names.size());
    vector<vector<double>> qhist_abs(col_names.size());
    vector<vector<size_t>> qhist_count(col_names.size());
    vector<vector<double>> acc_heights(col_names.size());
    for(size_t icol=0; icol< col_names.size(); ++icol) {
      for(size_t i=0; i< heights[icol].size(); ++(++i)) {
        acc_heights[icol].push_back(heights[icol][i]);
      }
      std::adjacent_difference(begin(acc_heights[icol]),end(acc_heights[icol]),std::back_inserter(qhist[icol]));
      double diff_abs = *std::max_element(begin(qhist[icol]),end(qhist[icol]))-*std::min_element(begin(qhist[icol]),end(qhist[icol]));
      for(auto i: qhist[icol]) qhist_abs[icol].push_back(i/diff_abs);
      double diff_rel = max[icol] - min[icol];
      //cout << diff_abs << "  " << diff_rel << "  " << count[icol] << endl;
      for(auto i: qhist[icol]) qhist_count[icol].push_back(static_cast<size_t>(i/diff_rel*count[icol]));
    }
    //const size_t qhist_plot_size=8*4;
    //for(size_t ih=0; ih<qhist_abs[0].size();++ih) {
      //cout << "qhist_"<<ih<<",";
      //for(size_t icol=0; icol<col_names.size();++icol){
        //size_t level = static_cast<size_t>(qhist_abs[icol][ih]*qhist_plot_size);
        //////cout << level;
        //for(unsigned i=0; i<level;i+=4) cout << '@';
        //if(level%4==1) cout << '(';
        //else if(level%4==2) cout << '|';
        //else  cout << ')';
        //if(i!=col_names.size()-1) cout << ',';
      //} cout << endl;
    //}
    //cout << endl;
    ///
    /// qhist done
    ///
    ///
    /// hist --- make splines  and get an average histogram
    ///
    double bin_size_div_2 = (binloc[i][1]-binloc[i][0])*0.5;
    vector<double>mbinloc;
    mbinloc.push_back(binloc[i].front()-3*bin_size_div_2);
    for(auto j: binloc[i]) mbinloc.push_back(j-bin_size_div_2);
    mbinloc.push_back(mbinloc.back()+2*bin_size_div_2);
    vector<double> wshist;
    wshist.push_back(0.0);
    for(auto j:whist[i]) wshist.push_back(j);
    wshist.push_back(0.0);
    tk::spline s; //  print out all bins or spline the beans and divide the spline to get approximate bins
    cout << "s( " << mbinloc.size() << "  " << wshist.size() <<")\n";
    for(auto j: mbinloc) cout << j << "  "; cout << endl;
    for(auto j: wshist) cout << j << "  "; cout << endl;
    s.set_points(mbinloc,wshist);
    double final_bin_size= (max[i]-min[i])/bins;
    cout << " final_bin_size: " << final_bin_size << endl;
    histloc[i][0]=min[i]+final_bin_size*0.5;
    hist[i][0]=s(static_cast<double>(histloc[i][0]));
    for(size_t j=1;j<bins;++j) {
      histloc[i][j]=histloc[i][j-1]+final_bin_size;
      hist[i][j]=s(static_cast<double>(histloc[i][j]));
    }
    for(size_t j=0;j<bins;++j) {
      cout << histloc[i][j] << "_" << hist[i][j]<< "   ";
    }
    cout << endl;
            for(auto j: binloc[i]) cout << j << " "; cout << endl;
            std::adjacent_difference(begin(binloc[i]),end(binloc[i]),std::ostream_iterator<double>(cout," ")); cout << endl;
            for(auto j: whist[i]) cout << j << " "; cout << endl;
            cout << std::accumulate(begin(whist[i]),end(whist[i]),0) << endl;
    ///
    /// hist done
    ///
  }

  cout << "acc_name,"; for(size_t i=0; i<col_names.size();++i){
    cout << col_names[i];
    if(i!=col_names.size()-1) cout << ',';
  } cout << endl;
  if(none_of(begin(missing),end(missing),[](auto m){return m==0?1:0;})){
    cout << "missing,"; for(size_t i=0; i<missing.size();++i){
      cout << missing[i];
      if(i!=col_names.size()-1) cout << ',';
    } cout << endl;
  }
  if(none_of(begin(infs),end(infs),[](auto m){return m==0?1:0;})){
    cout << "infs,"; for(size_t i=0; i<infs.size();++i){
      cout << infs[i];
      if(i!=col_names.size()-1) cout << ',';
    } cout << endl;
  }
  if(none_of(begin(nans),end(nans),[](auto m){return m==0?1:0;})){
    cout << "nans,"; for(size_t i=0; i<nans.size();++i){
      cout << nans[i];
      if(i!=col_names.size()-1) cout << ',';
    } cout << endl;
  }
  cout << "count,"; for(size_t i=0; i<missing.size();++i){
    cout << count[i];
    if(i!=col_names.size()-1) cout << ',';
  } cout << endl;
  //cout << "mean,"; for(size_t i=0; i<missing.size();++i){
    //cout << mean[i];
    //if(i!=col_names.size()-1) cout << ',';
  //} cout << endl;
  //cout << "m2,"; for(size_t i=0; i<missing.size();++i){
    //cout << sum2[i]/count[i];
    //if(i!=col_names.size()-1) cout << ',';
  //} cout << endl;
  //cout << "m3,"; for(size_t i=0; i<missing.size();++i){
    //cout << sum3[i]/count[i];
    //if(i!=col_names.size()-1) cout << ',';
  //} cout << endl;
  //cout << "std,"; for(size_t i=0; i<missing.size();++i){
    //cout << std[i];
    //if(i!=col_names.size()-1) cout << ',';
  //} cout << endl;
  //cout << "var,"; for(size_t i=0; i<missing.size();++i){
    //cout << var[i];
    //if(i!=col_names.size()-1) cout << ',';
  //} cout << endl;
  //cout << "ske,"; for(size_t i=0; i<missing.size();++i){
    //cout << ske[i];
    //if(i!=col_names.size()-1) cout << ',';
  //} cout << endl;
  //cout << "kur,"; for(size_t i=0; i<missing.size();++i){
    //cout << kur[i];
    //if(i!=col_names.size()-1) cout << ',';
  //} cout << endl;
  cout << "min,"; for(size_t i=0; i<missing.size();++i){
    cout << min[i];
    if(i!=col_names.size()-1) cout << ',';
  } cout << endl;
  //cout << "25%,"; for(size_t i=0; i<missing.size();++i){
    //cout << heights[i][1];
    //if(i!=col_names.size()-1) cout << ',';
  //} cout << endl;
  //for(size_t j=0; j<heights[0].size();++j){
    //if(j%2==0){
      //cout << "q" ;
      //if(j==0) cout << 0.0;
      //else if(j==heights[0].size()-1) cout << 1.0;
      //else cout << probabilities[(j-1)/2];
      //cout << ",";
      //for(size_t i=0; i<missing.size();++i){
        ////std::nth_element(begin(xv[i]),begin(xv[i])+xv[i].size()/2,end(xv[i]));
        ////cout << xv[i][xv[i].size()/2];
        //cout << heights[i][j];
        //if(i!=col_names.size()-1) cout << ',';
      //} cout << endl;
    //}else{
      ////cout << "q" ;
      ////if(j==0) cout << 0.0;
      ////else if(j==heights[0].size()-2) cout << (1.0+probabilities[(j-1)/2-1])/2;
      ////else cout << (probabilities[(j-1)/2]+probabilities[(j-1)/2-1])/2;
      ////cout << ",";
      ////for(size_t i=0; i<missing.size();++i){
        //////std::nth_element(begin(xv[i]),begin(xv[i])+xv[i].size()/2,end(xv[i]));
        //////cout << xv[i][xv[i].size()/2];
        ////cout << heights[i][j];
        ////if(i!=col_names.size()-1) cout << ',';
      ////} cout << endl;
    //}
  //}
  //cout << "50%,"; for(size_t i=0; i<missing.size();++i){
    ////std::nth_element(begin(xv[i]),begin(xv[i])+xv[i].size()/2,end(xv[i]));
    ////cout << xv[i][xv[i].size()/2];
    //cout << heights[i][2];
    //if(i!=col_names.size()-1) cout << ',';
  //} cout << endl;
  //cout << "75%,"; for(size_t i=0; i<missing.size();++i){
    //cout << heights[i][3];
    //if(i!=col_names.size()-1) cout << ',';
  //} cout << endl;
  cout << "max,"; for(size_t i=0; i<missing.size();++i){
    cout << max[i];
    if(i!=col_names.size()-1) cout << ',';
  } cout << endl;

  return 0;
}


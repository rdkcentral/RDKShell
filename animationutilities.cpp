/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2020 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#include <math.h>
#include <iostream>
#include <map>
#include <string>

namespace RdkShell
{

  typedef double (*interpolatorFunction)(double i);

  template <typename t>
  inline t minValue(t a1, t a2)
  {
    return (a1 < a2)?a1:a2;
  }

  template <typename t>
  inline t maxValue(t a1, t a2)
  {
    return (a1 > a2)?a1:a2;
  }

  template <typename t>
  inline double clamp(double v, double min, double max)
  {
    return minValue<t>(max, maxValue<t>(min, v));
  }

  double interpolateLinear(double i)
  {
    return clamp<double>(i, 0, 1);
  }

  double interpolateExponent1(double i)
  {
    return i*i;
  }

  double interpolateExponent2(double i)
  {
    return pow(i,0.48);
  }

  double interpolateExponent3(double i)
  {
    return pow(i,1.7);
  }

  double interpolateEaseInElastic(double t) 
  {
    double a = 0.8;
    double p = 0.5;
    double b = 0, c = 1, d = 1;

    if (t == 0) 
      return 0;

    double t_adj = t / d;
    if (t_adj == 1) 
      return b + c;

    double s;
    if (a < fabs(c)) {
      a = c;
      s = p / 4.0;
    } else {
      s = p / (2 * M_PI) * sin(c / a);
    }

    t_adj -= 1.0;

    return -(a * pow(2.0, 10 * t_adj) * sin((t_adj * d - s) * (2 * M_PI) / p)) + b;
  }

  double interpolateEaseOutElastic(double t) {
    if (t <= 0) return 0;
    if (t >= 1) return 1;

    double a = 0.8;
    double p = 0.5, c=1;
    double s;

    if (a < c) {
      a = c;
      s = p / 4.0;
    }
    else {
      s = p / (2*M_PI)*sin(c/a);
    }
    return (a * pow(2.0, -10*t) * sin((t-s)*(2.0*M_PI)/p)+c);
  }

  double interpolateEaseOutBounce(double t) 
  {
    double a = 1.0, c=1;
    if (t == 1.0) return c;
    if (t < (4 / 11.0)) {
      return c * (7.5625 * t * t);
    } else if (t < (8 / 11.0)) {
      t -= (6 / 11.0);
      return -a * (1. - (7.5625 * t * t + .75)) + c;
    } else if (t < (10 / 11.0)) {
      t -= (9 / 11.0);
      return -a * (1. - (7.5625 * t * t + .9375)) + c;
    } else {
      t -= (21 / 22.0);
      return -a * (1. - (7.5625 * t * t + .984375)) + c;
    }
  }

  double interpolateEaseInOutBounce(double t)
  {
    return interpolateEaseOutBounce(t);
  }

  double interpolateInQuad(double t)
  {
    return pow(t,2);
  }

  double interpolateInCubic(double t)
  {
    return pow(t,3);
  }

  double interpolateInBack(double t)
  {
    double s = 1.70158;
    return t*t*((s+1.0)*t-s);
  }

  static double pulseScale = 8;
  static double pulseNormalize = 1;

  static double computePulse(double x)
  {
  	double val;
  	x = x * pulseScale;
  	if (x < 1) {
            val = x - (1 - exp(-x));
  	} else {
            double start = exp(-1);
  	    x -= 1;
  	    double expx = 1 - exp(-x);
  	    val = start + (expx * (1.0 - start));
  	}

  	return val * pulseNormalize;
  }

  static void computePulseScale()
  {
  	pulseNormalize = 1.0 / computePulse(1);
  }

  double interpolateStop(double x)
  {
  	if (x >= 1) return 1;
  	if (x <= 0) return 0;

  	if (pulseNormalize == 1) {
  		computePulseScale();
  	}

  	return computePulse(x);
  }

  static std::map<std::string, interpolatorFunction> interpolatorFunctionMap = {};

  void initializeTweens()
  {
      interpolatorFunctionMap["linear"] = interpolateLinear; 
      interpolatorFunctionMap["exp1"] = interpolateExponent1; 
      interpolatorFunctionMap["exp2"] = interpolateExponent2; 
      interpolatorFunctionMap["exp3"] = interpolateExponent3; 
      interpolatorFunctionMap["stop"] = interpolateStop; 
      interpolatorFunctionMap["inquad"] = interpolateInQuad; 
      interpolatorFunctionMap["incubic"] = interpolateInCubic; 
      interpolatorFunctionMap["inback"] = interpolateInBack; 
      interpolatorFunctionMap["inelastic"] = interpolateEaseInElastic; 
      interpolatorFunctionMap["outelastic"] = interpolateEaseOutElastic; 
      interpolatorFunctionMap["outbounce"] = interpolateEaseOutBounce; 
  }

  interpolatorFunction interpolateFunction(std::string& tweentype)
  {
    std::map<std::string, interpolatorFunction>::iterator it = interpolatorFunctionMap.find(tweentype);
    if (it != interpolatorFunctionMap.end())
    {
      return it->second;
    }
    return interpolateLinear;
  }
}


//Called in main loop
void readSwitchesAndButtons(); 
void readSticks();   
void computeChannelOutputs(); 

//Helpers
int deadzoneAndMap(int _theInpt, int _minVal, int _centerVal, int _maxVal, int _deadzn, int _mapMin, int _mapMax);
int calcRateExpo(int _input_, int _rate_, int _expo_);
int linearInterpolate(int xValues[], int yValues[], int numValues, int pointX);
long weightAndOffset(int _input, int _weight, int _diff, int _offset);


//==================================================================================================
void readSwitchesAndButtons()
{
  uint8_t _val1, _val2, _val3;
  
  //Read keymatrix COLUMN 1, ROWS 1 & 2
  digitalWrite(COL3_MTRX_PIN, LOW);
  digitalWrite(COL2_MTRX_PIN, LOW);
  digitalWrite(COL1_MTRX_PIN, HIGH);
  SwAEngaged = !digitalRead(ROW1_MTRX_PIN); 
  _val1 = digitalRead(ROW2_MTRX_PIN);

  //Read keymatrix COLUMN 2, ROWS 1 & 2
  digitalWrite(COL1_MTRX_PIN, LOW);
  digitalWrite(COL2_MTRX_PIN, HIGH);
  SwBEngaged = digitalRead(ROW1_MTRX_PIN);
  _val2 = digitalRead(ROW2_MTRX_PIN);
  
  //Read keymatrix COLUMN 3, ROWS 1 & 2
  digitalWrite(COL2_MTRX_PIN, LOW);
  digitalWrite(COL3_MTRX_PIN, HIGH);
  SwDEngaged = digitalRead(ROW1_MTRX_PIN);
  _val3 = digitalRead(ROW2_MTRX_PIN);
  
  buttonCode = _val1 | _val2 << 1 | _val3 << 2;
  
  //SwCState gotten from slave mcu.
  SwCState = (returnedByte >> 6) & 0x03;
  
  
  ///----------- PLAY AUDIO IF SWITCHES MOVED -------------------
  uint8_t _switchesSum = SwAEngaged + SwBEngaged + SwDEngaged + SwCState;
  static uint8_t _lastSwitchesSum = _switchesSum;
  if(_switchesSum != _lastSwitchesSum)
    audioToPlay = AUDIO_SWITCHMOVED;
  _lastSwitchesSum = _switchesSum;
  
}

//==================================================================================================
void readSticks()
{
  /* Read, apply deadzones and limit. No deadzone for throttle input is applied as
    we wouldnt want it. Moreover the throttle curve feature can be used instead.
    Deadzone doesnt apply to the Knob (aux2In) as well*/
  if (isCalibratingSticks == true)
  {
    return;
  }

  rollIn = 0;
  pitchIn = 0;
  yawIn = 0;
  throttleIn = 0;
  aux2In = 0;

  #define _NUMSTICKSAMPLES 5
  for (int i = 0; i < _NUMSTICKSAMPLES; i += 1)
  {
    rollIn += analogRead(ROLLINPIN);
    pitchIn += analogRead(PITCHINPIN);
    throttleIn += analogRead(THROTTLEINPIN);
    yawIn += analogRead(YAWINPIN);
    aux2In += analogRead(AUX2PIN);
  }
  rollIn /= _NUMSTICKSAMPLES;
  pitchIn /= _NUMSTICKSAMPLES;
  throttleIn /= _NUMSTICKSAMPLES;
  yawIn /= _NUMSTICKSAMPLES;
  aux2In /= _NUMSTICKSAMPLES;

  rollIn = deadzoneAndMap(rollIn, rollMin, rollCenterVal, rollMax, deadZonePerc, -500, 500);
  rollIn = constrain(rollIn, -500, 500);

  pitchIn = deadzoneAndMap(pitchIn, pitchMin, pitchCenterVal, pitchMax, deadZonePerc, -500, 500);
  pitchIn = constrain(pitchIn, -500, 500);

  yawIn = deadzoneAndMap(yawIn, yawMin, yawCenterVal, yawMax, deadZonePerc, -500, 500);
  yawIn = constrain(yawIn, -500, 500);

  throttleIn = map(throttleIn, thrtlMin, thrtlMax, -500, 500);
  throttleIn = constrain(throttleIn, -500, 500);
  

  aux2In = map(aux2In, 20, 1003, -500, 500); /*Adds small deadband at ends for stability there. Without
  this deadband, usual range to map from would be 0 to 1023*/
  aux2In = constrain(aux2In, -500, 500);
  
  
  //play audio whenever knob crosses center 
  enum {_POS_SIDE = 0, _CENTER = 1, _NEG_SIDE = 2};
  static uint8_t _aux2Region = _CENTER;
  if(isCalibratingSticks == false)
  {
    int _aux2Cntr = 0;
    int _posSideStart =  25;  //has deadband
    int _negSideStart =  -25; //has deadband

    if((aux2In >= _aux2Cntr && _aux2Region == _NEG_SIDE) 
      || (aux2In < _aux2Cntr && _aux2Region == _POS_SIDE)) //crossed center
    {
      audioToPlay = AUDIO_SWITCHMOVED;
      _aux2Region = _CENTER;
    }
    else if(aux2In > _posSideStart)
      _aux2Region = _POS_SIDE;
    else if (aux2In < _negSideStart)
      _aux2Region = _NEG_SIDE;
  }
}

//==================================================================================================
void computeChannelOutputs()
{
  if (isCalibratingSticks == true)
  {
    return;
  }
  
  ///////////// CLEAR CHANNEL OUTPUTS TO ZERO ////////
  for(int i = 0; i < 8; i++)
    ChOut[i] = 0;
  
  //////////////////////// MIXER ///////////////////////////////////

  ///DECLARE MIX SOURCE ARRAY, INIT IT TO ZERO, THEN POPULATE IT AFTERWARDS 
  int MixSources[24];   //See the names in UI.
  for(int i = 0; i < 24; i++) 
    MixSources[i] = 0;
  
  enum{ //same order as the name order in UI.
        Idxswing = 0, 
        Idxroll, Idxpitch, Idxthrottle, Idxyaw, Idxknob, IdxSwB, IdxSwC, IdxSwD, IdxAil, IdxEle, IdxThrt, IdxRud,
        IdxNone, 
        IdxCh1, IdxCh2, IdxCh3, IdxCh4, IdxCh5, IdxCh6, IdxCh7, IdxCh8, IdxVrt1, IdxVrt2 };
        
  ///--Mix source - swing
  //-move back and forth btn -500 and 500. Period 2 seconds. Essentially a triangular wave
  int _sval = int(millis() % 2000);
  if(_sval <= 1000) //rise -500 to 500
    _sval = _sval - 500;
  else //fall 500 to -500
    _sval = 1500 - _sval;
  MixSources[Idxswing] = _sval;
  
  ///--Mix source sticks
  MixSources[Idxroll] = rollIn;
  MixSources[Idxpitch] = pitchIn;
  MixSources[Idxthrottle] = throttleIn;
  MixSources[Idxyaw] = yawIn;
  MixSources[Idxknob] = aux2In;
  
  ///--Mix source Switches B, C, and D
  
  enum {_DELTA_ = 60}; //Slowed to prevent abrupt change
  //Switch B
  static int _SwBVal = -500;
  if(SwBEngaged == true) _SwBVal += _DELTA_;
  else _SwBVal -= _DELTA_;
  _SwBVal = constrain(_SwBVal, -500, 500);
  MixSources[IdxSwB] = _SwBVal;
  //Switch C (3 pos)
  static int _SwCVal = 0;
  int _target;
  if(SwCState == SWLOWERPOS) _target = 500;
  else if(SwCState == SWUPPERPOS) _target = -500;
  else _target = 0;
  if(_target < _SwCVal) 
  { 
    _SwCVal -= _DELTA_;
    if(_SwCVal < _target)
      _SwCVal = _target;
  }
  else if(_target > _SwCVal) 
  {
    _SwCVal += _DELTA_;
    if(_SwCVal > _target)
      _SwCVal = _target;
  }
  MixSources[IdxSwC] = _SwCVal;
  //Switch D
  static int _SwDVal = -500;
  if(SwDEngaged == true) _SwDVal += _DELTA_;
  else _SwDVal -= _DELTA_;
  _SwDVal = constrain(_SwDVal, -500, 500);
  MixSources[IdxSwD] = _SwDVal;
  
  
  ///--Mix source Ail, Ele, Thr, Rudd
  //apply rate and expo 
  if(SwBEngaged == false || DualRateEnabled[AILRTE] == false)
    MixSources[IdxAil]  = calcRateExpo(rollIn,  RateNormal[AILRTE], ExpoNormal[AILRTE]);
  else
    MixSources[IdxAil]  = calcRateExpo(rollIn,  RateSport[AILRTE], ExpoSport[AILRTE]);
  if(SwBEngaged == false || DualRateEnabled[ELERTE] == false)
    MixSources[IdxEle]  = calcRateExpo(pitchIn, RateNormal[ELERTE], ExpoNormal[ELERTE]);
  else
    MixSources[IdxEle]  = calcRateExpo(pitchIn, RateSport[ELERTE], ExpoSport[ELERTE]);
  if(SwBEngaged == false || DualRateEnabled[RUDRTE] == false)
    MixSources[IdxRud] = calcRateExpo(yawIn,   RateNormal[RUDRTE], ExpoNormal[RUDRTE]);
  else
    MixSources[IdxRud] = calcRateExpo(yawIn,   RateSport[RUDRTE], ExpoSport[RUDRTE]);
  //apply throttle curve
  int xpoints[5] = {-500, -250, 0, 250, 500};
  int ypoints[5];
  for(int i = 0; i < 5; i++)
    ypoints[i] = ((int)ThrottlePts[i] - 100) * 5;
  
  MixSources[IdxThrt] = linearInterpolate(xpoints, ypoints, 5, throttleIn);

  ///Predefined mixes
  //So we dont waste the limited mixer slots
  ChOut[CH1OUT] = MixSources[IdxAil];  //send Ail  to Ch1
  ChOut[CH2OUT] = MixSources[IdxEle];  //send Ele  to Ch2
  ChOut[CH3OUT] = MixSources[IdxThrt];  //Send Thrt to Ch3
  ChOut[CH4OUT] = MixSources[IdxRud]; //Send Rud  to Ch4
  
  ///-- Other Mix sources --
  MixSources[IdxCh1]  =  ChOut[CH1OUT]; 
  MixSources[IdxCh2]  =  ChOut[CH2OUT];
  MixSources[IdxCh3]  =  ChOut[CH3OUT];
  MixSources[IdxCh4]  =  ChOut[CH4OUT];

  ///FREE MIXER
  //Takes two input sources, adds or multiplies them, then writes result to specified output
  for(int _mixNum = 0; _mixNum < NUM_MIXSLOTS; _mixNum++)
  {
    //---Input1---
    long _operand1 = 0;
    if(MixIn1[_mixNum] != IdxNone) //only calculate if input is other than "None"
    {
      _operand1 = weightAndOffset(MixSources[MixIn1[_mixNum]], MixIn1Weight[_mixNum], 
                                  MixIn1Diff[_mixNum], MixIn1Offset[_mixNum]);
    }
    //---Input2---
    long _operand2 = 0;
    if(MixIn2[_mixNum] != IdxNone) //only calculate if input is other than "None"
    {
      _operand2 = weightAndOffset( MixSources[MixIn2[_mixNum]], MixIn2Weight[_mixNum], 
                                   MixIn2Diff[_mixNum], MixIn2Offset[_mixNum]);
    }
    //---Mix the two inputs---
    long _output;      
    if(MixOperator[_mixNum] == 0) //add
      _output = _operand1 + _operand2; 
    else //multiply
      _output = (long(_operand1)* long(_operand2))/500; 
    //---Clamp-----
    _output = constrain(_output, -500, 500);
    //---Update sources array for next iteration--- 
    MixSources[MixOut[_mixNum]] = int(_output);
  }
  
  //////////////////////// OUTPUT ///////////////////////////
  //Write to real output channels, apply reverse, subtrim, cut, endpoints
  for(int i = 0; i < 8; i++)
  {
    //Write to the real channels
    ChOut[i] = MixSources[IdxCh1 + i];
    
    //write mixer outputs for graphing
    ChOutMixer[i] = ChOut[i] / 5; //divide by 5 to fit datatype
    
    //Apply Reverse
    if (Reverse[i] == true) 
      ChOut[i] = 0 - ChOut[i]; 
    
    //Apply Subtrim
    ChOut[i] += 5 * (Subtrim[i] - 50); 
    
    //Check Cut. If specified and switch engaged, overide
    if(SwAEngaged == true && CutValue[i] > 0)
      ChOut[i] = 5 * (CutValue[i] - 101);
    
    //Apply endpoints
    ChOut[i] = constrain(ChOut[i], 5 * (0 - EndpointL[i]), 5 * EndpointR[i]); 
  }
}

//====================================Helpers=======================================================

int deadzoneAndMap(int _theInpt, int _minVal, int _centerVal, int _maxVal, int _deadzn, int _mapMin, int _mapMax)
{
  long _ddZnTmp = (long)(_maxVal - _minVal) * _deadzn;
  _ddZnTmp /= 100;
  int _ddZnVal = int(_ddZnTmp) / 2; //divide by 2 as we apply deadzone about center
  
  int _mapCenter = (_mapMin / 2) + (_mapMax / 2);
  
  //add dead zone and map the input
  int _outpt;
  if (_theInpt > _centerVal + _ddZnVal)
    _outpt = map(_theInpt, _centerVal + _ddZnVal, _maxVal, _mapCenter + 1, _mapMax);
  else if (_theInpt < _centerVal - _ddZnVal)
    _outpt = map(_theInpt, _minVal, _centerVal - _ddZnVal, _mapMin, _mapCenter - 1);
  else
    _outpt = _mapCenter;

  return _outpt;
}


int calcRateExpo(int _input_, int _rate_, int _expo_)
{
  /* This function is for applying rate and cubic 'expo' to aileron, elevator and rudder channels.
     Ranges: 
     _input_  -500 to 500, 
     _rate_   0 to 100
     _expo_   0 to 200. 100 is linear, < 100 is negative expo, > 100 is positive expo 
  */
 
  /**
    The cubic equation used is 
    y = k*x + (1-k)*x^3  where k is expo factor. 
    This equation applies for input output ranges from -1 to +1. 
    k = 1 is linear. 
    0 < k < 1    is less sensitive in center and more sensitive at ends.  
    1 < k <= 1.5 is more sensitive in center and less senstive at outside.
    
    For our implementation, we only use the range 0 < k < 1 and simply 'invert' the result if 
    positive expo is specified.
    
    We need to scale up this equation to avoid floating point maths. After calculation, we scale back
    the result. 
    Thus the modified equation taking into account the range of our parameters (to ensure no overflow)
    is as follows. 
    
    y = ( (k/100 * x/500)  +  ((1 - k/100)*((x*x*x)/(500*500*500))) ) * 500
    This simplifies to 
    y =  (((250000*k  + x*x*(100-k)) / 250000 ) * x) / 100
    To ensure no significant loss of precision, we only work with positive values of x
  */
  
  long x = _input_;
  if(_input_ < 0) x = -x;
    
  long k = _expo_;
  if(_expo_ > 100)
  {
    k = 200 - k;
    x -= 500;
  }

  //apply expo factor
  long y = 250000L * k;
  y += ((x*x)*(100-k));
  y /= 250000L;
  y *= x;
  y /= 100;
  if(_expo_ > 100) y += 500;

  //apply rate
  long rate = _rate_;
  y *= rate;
  y /= 100;
  
  if(_input_ < 0) y = -y;
    
  return int(y);
}


long weightAndOffset(int _input, int _weight, int _diff, int _offset)
{
  //map passed values
  _weight -= 100; 
  _diff -= 100;
  _offset = (_offset - 100) * 5;
  //calc left and right weights basing on differential
  int _wtR, _wtL; 
  if(_diff >= 0)
  {
    _wtR = _weight;
    _wtL = (_weight * (100 - _diff))/100;
  }
  else 
  {
    _wtL = _weight;
    _wtR = (_weight * (100 + _diff))/100;
  }
  //Compute output and add offset
  _weight = _input > 0 ? _wtR : _wtL;
  long _outVal = _input;
  _outVal *= _weight;
  _outVal /= 100;
  _outVal += _offset;
  return _outVal;
}


int linearInterpolate(int xValues[], int yValues[], int numValues, int pointX)
{
  //Implementation of linear Interpolation using integers
  //Formula used is y = ((x - x0)*(y1 - y0))/(x1 - x0) + y0;
	
	long y = 0;
	
	if(pointX < xValues[0])
    y = yValues[0]; 
	else if(pointX > xValues[numValues - 1])
		y = yValues[numValues - 1];
	else  //point is in range, interpolate
	{ 
	  for(int i = 0; i < numValues - 1; i++)
    {
      if(pointX >= xValues[i] && pointX <= xValues[i+1])
      {
        long x0 = xValues[i];
        long x1 = xValues[i+1];
        long y0 = yValues[i];
        long y1 = yValues[i+1];
        long x = pointX;
        y = (((x - x0) * (y1 - y0)) + (y0 * (x1 - x0))) / (x1 - x0);
	  		break;
      }
    }
	}

	return int(y); 
}

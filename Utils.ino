//----------------------------------------------------------------------
// print
void Print(String input) {
#ifdef DEBUG
  Serial.print(input);
#endif
}


//----------------------------------------------------------------------
// print
void Println(String input) {
#ifdef DEBUG
  Serial.println(input);
#endif
}


//----------------------------------------------------------------------
// Map input to output:  input, inMin, inMax, outMin, OutMax
// This will change the input range to the wanted output range.
// Make sure that (inMax - inMin) + outMin is not zero.
float MapFloat(float input, float inMin, float inMax, float outMin, float outMax) {
  float result;
  float divisor = inMax - inMin + outMin;

  if (divisor == 0.0) {
    divisor = 0.00001;
    result = ((input - inMin) * (outMax - outMin)) / divisor;
  } else {
    result = (input - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
  }

  return result;
}


//---------------------------------------------------------------
// STRINGS_SIZE 30    aStrings[STRINGS_SIZE];
// string: string to parse
// returns number of items parsed
int split(char splitChar, String msgString, String strings[]) {
  int bufferIndex = 0;
  strings[bufferIndex] = "";

  for (int index = 0; index < msgString.length(); ++index) {
    if (msgString[index] != splitChar) {
      strings[bufferIndex] += msgString[index];
    } else {
      bufferIndex++;
      if (bufferIndex >= STRINGS_SIZE) {
        break;
      } else {
        strings[bufferIndex] = "";
      }
    }
  }

  bufferIndex++;

  return bufferIndex;
}

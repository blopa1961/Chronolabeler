const char PaginaWeb[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<link rel="icon" href="/favicon32.png" sizes="32x32" type="image/png">
<link rel="icon" href="/favicon192.png" sizes="192x192" type="image/png">
<title>Chrono Labeler</title>
<script>
setInterval(function() {
  getEstado();
}, 500);

function getEstado() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) document.getElementById("estado").innerHTML = this.responseText;
  };
  xhttp.open("GET", "updEstado", true);
  xhttp.send();
}
</script>
<style>
p {
  display: block;
  margin-top: 0;
  margin-bottom: 0;
  margin-left: 0;
  margin-right: 0;
}
button[type="submit"] {
  height: 50px;
  width: 200px;
  font-size: 20px;
}
</style>
</head>
<body>
<p style="font-family:Arial;font-size:30px"><b>Chrono Labeler</b><br><br></p>
<form action="/" method="POST">
  <p style="font-size:20px"><b>Profile:</b><br>
  <select style="font-size:17px" name="selPrn" onchange="this.form.submit()">
  PULLOPTIONS
  </select><br>
  <p style="font-size:20px"><b>Status: </b><span id="estado">Disconnected</span></p><br>
  <input type="text" style="font-size:20px" maxlength="20" id="linea1" name="line0" placeholder= "Text to print (line 1)" value="MYLINE0"><br><br>
  <input type="text" style="font-size:20px" maxlength="20" id="linea2" name="line1" placeholder= "Text to print (line 2)" value="MYLINE1"><br><br>
  <p style="font-size:20px"><b>Line 1 Font:</b><br>
  <select style="font-size:17px" name="pullf0">
  PULLFR0
  </select><br><br>
  <p style="font-size:20px"><b>Line 2 Font:</b><br>
  <select style="font-size:17px" name="pullf1">
  PULLFR1
  </select><br><br></p>
  <p style="font-size:20px"><b>
  <label for="centro">Center texts:&nbsp</label>
  <input type="checkbox" id="centro" name="centrar" CENTERCHECK><br><br>
  <label>Background: </label></b>
  <input type="radio" id="blanco" name="color" value="0"CHECKWHITE>
  <label for="blanco">White</label>
  <input type="radio" id="negro" name="color" value="1"CHECKBLACK>
  <label for="negro">Black</label><br><br><br></p><b>
  &nbsp&nbsp<button type="submit" formaction="/preview" name= "prvw" value="7">Preview</button><br><br>
  &nbsp&nbsp<button type="submit" name="accion" value="1" PRINTENA>Print!</button><br><br>
  </form>
  <br><p style="font-size:20px"><b><a href="/configPrn">Configure Printer</a></b>
  <br><br><b><a href="/configDate">Configure Date and Time</a></b>
  <br><br><b><a href="/configWifi">Configure WiFi</a></b>
  <br><br><b><a href="/about">About</a></b></p>
  <br><p style="font-size:17px"><a href="http://www.montoreano.com">&copy; 2026 Pablo Montoreano</a></p>
</body>
</html>
)=====";

const char ConfPrn[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>Printer Configuration</title>
<style>
p {
  display: block;
  margin-top: 0;
  margin-bottom: 0;
  margin-left: 0;
  margin-right: 0;
}
button[type="submit"] {
  height: 50px;
  width: 200px;
  font-size: 20px;
}
input[type="button"] {
  height: 50px;
  width: 200px;
  font-size: 20px;
}
input[type="text"] {
  font-size: 15px;
}
table, th, td {border: 1px solid black;  border-collapse: collapse;}
</style>
</head>
<body>
<p style="font-size:25px"><b>Printer Configuration</b></p><br>
<form action="/configPrn" method="POST">
  <select style="font-size:15px" name="selCfgPrn" onchange="this.form.submit()">
  PULLCFG
  </select><br>
</form>
<form action="/" method="POST">
  <p style="font-size:20px"><b>
  <label for="elprofile">Printer Profile NUMPERF</label><br>
  <input type="text" maxlength="25" id="elprofile" name="myProfile" placeholder= "Printer Profile" value="myPROF"><br>
  <label for="elname">Bluetooth Name:</label><br>
  <input type="text" maxlength="29" id="elname" name="myBleName" placeholder= "Printer BLE name" value="myNAME"><br>
  <label>Tipo: </label></b>
  <input type="radio" id="portr" name="prnType" value="0"CHECKPORT>
  <label for="portr">Portrait</label>
  <input type="radio" id="landsc" name="prnType" value="1"CHECKLAND>
  <label for="landsc">Landscape</label><br><b>
  <label>Connection:</label></b>
  <span id="vincu">PAIRED&nbsp;Paired</span><br><br><b>
  <label>Line 1 Font:</label><br></b>
  <select style="font-size:15px" name="pullf0">
  PULLFR0
  </select><br><b>
  <label>Line 2 Font:</label><br></b>
  <select style="font-size:15px" name="pullf1">
  PULLFR1
  </select><br>
  <p style="font-size:20px">
  <b><label>Background: </label></b>
  <input type="radio" id="blanco" name="color" value="0"CHECKWHITE>
  <label for="blanco">White</label>
  <input type="radio" id="negro" name="color" value="1"CHECKBLACK>
  <label for="negro">Black</label><br><br><b>
  <label for="elwidthmm">Width (mm):</label><br>
  <input type="number" min="30" max="56" id="elwidthmm" name="anchomm" placeholder= "Width in mm" value="WMM"><br>
  <label for="elheightmm">Height (mm):</label><br>
  <input type="number" min="12" max="25" id="elheightmm" name="altomm" placeholder= "Height in mm" value="HMM"><br>
  <label for="elpiemm">Blank footer (mm):</label><br>
  <input type="number" min="0" max="80" id="elpiemm" name="piemm" placeholder= "Footer (formfeed)" value="EXFT"><br>
  <label for="elmargensup1">Top margin for line 1 (pixels):</label><br>
  <input type="number" min="0" max="MAXSUP" id="elmargensup1" name="margensup1" placeholder= "Top margin (line 1)" value="MS1"><br>
  <label for="elmargensup2">Top margin for line 2 (pixels):</label><br>
  <input type="number" min="0" max="MAXSUP" id="elmargensup2" name="margensup2" placeholder= "Top margin (line 2)" value="MS2"><br>
  <label for="elmargenizq">Left Margin (pixels):;</label><br>
  <input type="number" min="0" max="200" id="elmargenizq" name="margenizq" placeholder= "Left margin" value="MIZQ">&nbsp&nbsp
  <input type="checkbox" id="centro" name="centrar" CENTERCHECK></b>
  <label for="centro">Center</label><br><b>
  <label for="elchunkdelay">BLE packet delay (ms):</label><br>
  <input type="number" min="0" max="50" id="elchunkdelay" name="chunkdelay" placeholder= "BLE packet delay (ms)" value="CDELAY"><br>
  <label for="elresetdelay">Reset Delay (ms):</label><br>
  <input type="number" min="0" max="9999" id="elresetdelay" name="resetdelay" placeholder= "Reset delay (ms)" value="RDELAY"><br>
  <br><br>
  <button type="submit" formaction="/preview" name= "prvw" value="8">Preview</button><br><br>
  <button type="submit" name="accion" value="2">Save</button><br><br>
  <button type="submit" name="accion" value="3">Delete</button><br><br>
  <button type="submit" name="accion" value="4">Cancel</button><br>
</form>
PLACEHLD
</body>
</html>
)=====";

const char ConfDate[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
<title>Date and Time Configuration</title>
<style>
p {
  display: block;
  margin-top: 0;
  margin-bottom: 0;
  margin-left: 0;
  margin-right: 0;
}
input[type="button"] {
  height: 50px;
  width: 220px;
  font-size: 20px;
}
button[type="submit"] {
  height: 50px;
  width: 220px;
  font-size: 20px;
}
input[type="text"] {
  font-size: 15px;
}
</style>
</head>
<body>
<p style="font-size:25px"><b>Date and Time Configuration</b></p><br>
<form action="/" method="POST">
  <p style="font-size:20px"><b>Date Format:</b></p>
  <p style="font-size:15px">
  <select name="pulldt0">
  PULLDR0
  </select>
  &nbsp;
  <select name="pulldt1">
  PULLDR1
  </select>
  &nbsp;
  <select name="pulldt2">
  PULLDR2
  </select><br></p>
  <p style="font-size:20px"></b>Date Separators:<br></p>
  <p style="font-size:15px">
  <select name="pulldt5">
  PULLSEP0
  </select></p>
  <p style="font-size:20px">
  <input type="checkbox" id="diasem" name="diasema" DIASECHECK>
  <label for="diasem">Day of Week</label><br><br>
  <p style="font-size:20px"><b>Time Format:</b></p>
  <p style="font-size:15px">
  <select name="pulldt3">
  PULLTR0
  </select>
  &nbsp;
  <select name="pulldt4">
  PULLTR1
  </select><br></p>
  <p style="font-size:20px"></b>Time Separators:&nbsp;</p>
  <p style="font-size:15px">
  <select name="pulldt6">
  PULLSEP1
  </select><br><br>
  <b></p>
  <p style="font-size:20px">
  <label for="elntp"><b>NTP Server:</b></label><br>
  <input type="text" style="font-size:15px" maxlength="30" id="elntp" name="myNtp" placeholder= "NTP Server" value="NTPR"><br></b>
  <p style="font-size:20px">You may select one of these:<br>
  <select style="font-size:15px" name="pullntp" onchange="document.getElementById('elntp').value= this.options[this.selectedIndex].text">
  PULLNTP
  </select><br><br><b>
  <label for="eltzone">NTP Time Zone:</label><br>
  <input type="text" style="font-size:15px" maxlength="20" id="eltzone" name="myTZ" placeholder= "NTP Time Zone" value="NTPTZR"><br>
  <label for="elhuso">Time Zone (UTC):</label><br>
  <input type="number" style="font-size:15px" min="-12" max="14" id="elhuso" name="huso" placeholder= "Time Zone" value="HUSOR"><br>
  <label for="elverano">Daylight Saving Time:</label><br>
  <input type="number" style="font-size: 15px" min="-1" max="2" id="elverano" name="verano" placeholder= "Daylight Saving Time" value="VERAR"><br><br>
  <button type="submit" formaction="/preview" name= "prvw" value="9">Preview</button><br><br>
  <button type="submit" name="accion" value="5">Save</button><br><br>
</form>
  <input type="button" value="Cancel" onclick="window.location.href='/'"><br>
</body>
</html>
)=====";

const char Preview[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
</head>
<style>
p {
  display: block;
  margin-top: 0;
  margin-bottom: 0;
  margin-left: 0;
  margin-right: 0;
}
input[type="button"] {
  height: 50px;
  width: 120px;
  font-size: 20px;
}
</style>
<body>
<p style="font-family:Arial;font-size:30px"><b>Preview</b><br><br></p>
<img src="/bitmap.bmp" width="myWI" height="myHE">
<br><br><br>&nbsp;&nbsp;
<input type="button" style="font-size:20px" value="Accept" onclick="history.go(-1)">
</body>
</html>
)=====";

const char badParam[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
</head>
<style>
p {
  display: block;
  margin-top: 0;
  margin-bottom: 0;
  margin-left: 0;
  margin-right: 0;
}
input[type="button"] {
  height: 50px;
  width: 120px;
  font-size: 20px;
}
</style>
<body>
<p style="font-family:Arial;font-size:30px"><b>Chrono Labeler</b><br></p>

<br><p style="font-size:20px">Invalid Parameter: BADPAR<br><br>
<br>&nbsp;&nbsp;<input type="button" value="Accept" onclick="window.location.replace('/configPrn')">
</body>
</html>
)=====";
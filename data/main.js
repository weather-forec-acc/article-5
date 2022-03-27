// ----------------------------------------------------------------------------
// ----- 1. Текущий ток, потребляемый устройством -----------------------------
// ----------------------------------------------------------------------------
function getDeviceCurrent() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
		// readyState = 4: запрос завершен и ответ готов, 200: "ОК"
        if (this.readyState == 4 && this.status == 200) { 
            document.getElementById("device_current").innerHTML = this.responseText;
        }
    };
    xhttp.open("GET", "/device_current_init", true);
    xhttp.send();
};

// ----------------------------------------------------------------------------
// ----- 3. Запрос и установка температуры радиатора --------------------------
// ----------------------------------------------------------------------------
function getHeatsinkTemperature() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            //console.log("Запрос т-ры: ");
			//console.log(this.responseText);
			var myObj = JSON.parse(this.responseText);
			//console.log(myObj);
			plotTemperature(myObj);
        }
    };
    xhttp.open("GET", "temperature_init", true);
    xhttp.send();
};

// ----------------------------------------------------------------------------
// ----- 4. Запрос дней, часов и минут работы МК ------------------------------
// ----------------------------------------------------------------------------
function operating_time_init() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
			operation_time_show(this.responseText);
        }
    };
    xhttp.open("GET", "operating_time_init", true);
    xhttp.send();
}

function operation_time_show(response) {
	var myObj = JSON.parse(response);
    document.getElementById("operating_time_days").innerHTML = myObj["days"] + "<rt>days<rt>";
	document.getElementById("operating_time_hours").innerHTML = myObj["hours"] + "<rt>hours<rt>";
	document.getElementById("operating_time_mins").innerHTML = myObj["mins"] + "<rt>mins<rt>";
}

// ----------------------------------------------------------------------------
// ----- 5.Функция "Увеличить частоту" ----------------------------------------
// ----------------------------------------------------------------------------
function inc_freq() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            document.getElementById("set_frequency").innerHTML = this.responseText;
        }
    };
    xhttp.open("GET", "freq_inc", true);
    xhttp.send();
}

// ----------------------------------------------------------------------------
// ----- 6. Функция "Уменьшить частоту" ---------------------------------------
// ----------------------------------------------------------------------------
function dec_freq() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            document.getElementById("set_frequency").innerHTML = this.responseText;
        }
    };
    xhttp.open("GET", "freq_dec", true);
    xhttp.send();
}

// ----------------------------------------------------------------------------
// ----- 7. Вывод текущего времени --------------------------------------------
// ----------------------------------------------------------------------------
const getCurrentTimeDate = () => {
    let currentTimeDate = new Date();
    var weekday = new Array('Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat');
    var month = new Array('Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec');
    var hours = currentTimeDate.getHours();
    var minutes = currentTimeDate.getMinutes();
    minutes = minutes < 10 ? '0' + minutes: minutes;
    var currentTime = `${hours}:${minutes}`;
    var currentDay = weekday[currentTimeDate.getDay()];
    var currentDate = currentTimeDate.getDate();
    var currentMonth = month[currentTimeDate.getMonth()];
    var CurrentYear = currentTimeDate.getFullYear();
    var fullDate = `${currentDate} ${currentMonth} ${CurrentYear}`;
    document.getElementById("time").innerHTML = currentTime;
    document.getElementById("day").innerHTML = currentDay;
    document.getElementById("date").innerHTML = fullDate;
    setTimeout(getCurrentTimeDate, 30000);
}

// --------------------------------------------------------------------------------------
// ----- 8. Запрос на переход в режим настройки. Запускаем периодический запрос тока ----
// --------------------------------------------------------------------------------------
function run_setup() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            // Начинается настройка. Делаем недоступной кнопку Run setup
            document.getElementById("run_setup_btn_id").setAttribute("disabled", "disabled");
			series = chartC.series[0];
			while(series.data.length > 0)
				series.data[0].remove();
        }
    };
    xhttp.open("GET", "run_setup", true);
    xhttp.send();
}

// ----------------------------------------------------------
// ----- 9. Функции, выполняемые после загрузки страницы ----
// ----------------------------------------------------------
window.onload = function () {
    // 1. Первоначальный запрос и установка частоты
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            document.getElementById("set_frequency").innerHTML = this.responseText;
        }
    };
    xhttp.open("GET", "set_frequency", true);
    xhttp.send();
    // 2. Устанавливаем дату и время компьютера
    getCurrentTimeDate();
    // 3. Запрашиваем и устанавливаем данные датчиков температуры
    getHeatsinkTemperature();
    // 4. Запрашиваем и устанавливаем ток устройства
    getDeviceCurrent();
    // 5. Запрашиваем и устанавливаем дату начала работы МК
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function () {
        if (this.readyState == 4 && this.status == 200) {
            document.getElementById("start_operation").innerHTML = this.responseText;
        }
    };
    xhttp.open("GET", "start_operation", true);
    xhttp.send();
    // 6. Запрашиваем и устанавливаем время работы МК
    //operating_time();
	operating_time_init();
};

if (!!window.EventSource) {
  var source = new EventSource('/events');
  source.addEventListener('open', function(e) {
    console.log("Events Connected");
	document.getElementById("run_setup_btn_id").disabled = false;
  }, false);
  
  source.addEventListener('error', function(e) {
    if (e.target.readyState != EventSource.OPEN) {
      console.log("Events Disconnected");
	  console.log(e);
    }
  }, false);

  source.addEventListener('message', function(e) {
    console.log("message", e.data);
  }, false);

  // Прослушивание события "Передача времени наработки МК"
  source.addEventListener('operating_time_new', function(e) {
	//console.log("operating_time_new: ", e.data);
    operation_time_show(e.data);
  }, false);
  
  // Прослушивание события "Передача показаний датчиков температуры"
  source.addEventListener('temperature_new', function(e) {
	//console.log("temperature_new: ", e.data);
    //operation_time_show(e.data);
	var myObj = JSON.parse(e.data);
    //console.log(myObj);
    plotTemperature(myObj);
  }, false);
  
  // Прослушивание события "Передача тока устройства"
  source.addEventListener('device_current_new', function(e) {
	//console.log("device_current_new: ", e.data);
    device_current_show(e.data);
  }, false);
  
  // Прослушивание события "Передача в режиме настройки частоты и тока устройства"
  source.addEventListener('setup_current_new', function(e) {
	//console.log("setup_current_new: ", e.data);
	// Расшифровываем принятую строку
	var myObj = JSON.parse(e.data);
	var frequency = parseInt(myObj["freq"]);
	var current = parseInt(myObj["curr"]);
	// Если принят ток 32000, значит поступила оптимальная частота.
	if (current < 32000) {
		// Отображаем принятую частоту и соответствующий ей ток
		document.getElementById("set_frequency").innerHTML = frequency;
		document.getElementById("device_current").innerHTML = current;	
		//console.log(myObj);
		plotSetupCurrent(myObj);
	} else {
		// Получена оптимальная частота
	    // Стираем значение тока (теперь периодически будем получать текущие)
		document.getElementById("set_frequency").innerHTML = frequency;		
		document.getElementById("device_current").innerHTML = "Wait...";
		document.getElementById("run_setup_btn_id").disabled = false;
	}
  }, false);
}

function device_current_show(response) {
  document.getElementById("device_current").innerHTML = response;
}

// Create Temperature Chart
var chartT = new Highcharts.chart('chart-temperature',{
  chart: {
    type: 'spline',
    inverted: false
  },
  time: {
    // timezoneOffset: -120
	useUTC: false,
	timezone: 'Europe/Helsinki'
  },
  series: [
    {
      name: 'Heatsink',
      type: 'line',
      color: '#FF0000',
      marker: {
        symbol: 'circle',
        radius: 3,
        fillColor: '#FF0000',
      }
    },
    {
      name: 'Ambient',
      type: 'line',
      color: '#009E00',
      marker: {
        symbol: 'square',
        radius: 3,
        fillColor: '#009E00',
      }
    },
    {
      name: '3rd sensor',
      type: 'line',
      color: '#0000FF',
      marker: {
        symbol: 'triangle',
        radius: 3,
        fillColor: '#0000FF',
      }
    },
  ],
  title: {
    text: undefined
  },
  xAxis: {
    type: 'datetime',
    dateTimeLabelFormats: { second: '%H:%M:%S' }
  },
  yAxis: {
    title: {
      text: 'Temperature Celsius Degrees'
    }
  },
  credits: {
    enabled: false
  },
  plotOptions: {
        spline: {
            marker: {
                enable: false
            }
        }
  },
  legend: {
    itemStyle: {
      fontWeight: 'normal'
    }
  },
  tooltip: {
        headerFormat: '<b>{series.name}</b><br/>',
        pointFormat: '{point.x:%H:%M:%S}: {point.y}°C'
		//pointFormat: "Fecha: {point.x:%H:%M:%S} date, <br>Evento: {point.myData}  "
  }
});


//Plot temperature in the temperature chart
function plotTemperature(jsonValue) {
  var keys = Object.keys(jsonValue);
  //console.log(keys);
  //console.log(keys.length);

  for (var i = 0; i < keys.length; i++){
    var x = (new Date()).getTime();
    //console.log(x);
    const key = keys[i];
    var y = Number(jsonValue[key]);
    //console.log(y);

    if(chartT.series[i].data.length > 40) {
      chartT.series[i].addPoint([x, y], true, true, true);
    } else {
      chartT.series[i].addPoint([x, y], true, false, true);
    }

  }
}

// Настройка: график тока
var chartC = new Highcharts.chart('chart-current',{
  chart: {
    type: 'spline',
    inverted: false
  },
  time: {
        timezoneOffset: -120
  },
  series: [
    {
      name: 'Current',
      type: 'line',
      color: '#FF0000',
      marker: {
        symbol: 'circle',
        radius: 3,
        fillColor: '#FF0000',
      }
    }
  ],
  title: {
    text: undefined
  },
  xAxis: {
	title: {
      text: 'Frequency, MHz'
    }
  },
  yAxis: {
    title: {
      text: 'Current, A'
    }
  },
  legend: {
    enabled: false
  },
  credits: {
    enabled: false
  },
  tooltip: {
    headerFormat: '<b>{series.name}</b><br/>',
    pointFormat: '{point.x} MHz: {point.y} A'
  }
});

// Настройка: построение графика тока
function plotSetupCurrent(jsonValue) {
  var keys = Object.keys(jsonValue);
  var freq = Number(jsonValue[keys[0]])/1000000.;
  var curr = Number(jsonValue[keys[1]])/1000.;
  //console.log(freq, curr);
  chartC.series[0].addPoint([freq, curr], true, false, true);
}
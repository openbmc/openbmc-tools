function OpenFileHandler() {
  console.log('Will open a dialog box ...');
  const options = {
    title: 'Open a file or folder',
  };
  let x = dialog.showOpenDialogSync(options) + '';  // Convert to string
  console.log('file name: ' + x)

  // Determine file type
  let is_asio_log = false;
  const data = fs.readFileSync(x, {encoding: 'utf-8'});
  let lines = data.split('\n');
  console.log('This file has ' + lines.length + ' lines');
  for (let i = 0; i < lines.length; i++) {
    if (lines[i].indexOf('@asio') == 0) {
      is_asio_log = true;
      break;
    }
  }

  if (is_asio_log) {
    ShowBlocker('Loading Boost ASIO handler tracking log');
    console.log('This file is a Boost Asio handler tracking log');
    ParseBoostHandlerTimeline(data);
    OnGroupByConditionChanged_ASIO();
    if (boost_asio_handler_timeline_view.IsEmpty() == false) {
      boost_asio_handler_timeline_view.CurrentFileName = x;
      HideWelcomeScreen();
      ShowASIOTimeline();
      ShowNavigation();
      UpdateFileNamesString();
    }
    HideBlocker()
    return;
  }


  // First try to parse using dbus-pcap
  var dbus_pcap_out1,
      dbus_pcap_out2;  // Normal format and JSON format output from dbus-pcap
  ShowBlocker('Running dbus-pcap, pass 1/2');
  exec(
      'python3 dbus-pcap ' + x, {maxBuffer: 1024 * 1024 * 64},  // 64 MB buffer
      (error, stdout, stderr) => {
        console.log('stdout len:' + stdout.length);
        dbus_pcap_out1 = stdout;
        ShowBlocker('Running dbus-pcap, pass 2/2');
        exec(
            'python3 dbus-pcap --json ' + x, {maxBuffer: 1024 * 1024 * 64},
            (error1, stdout1, stderr1) => {
              dbus_pcap_out2 = stdout1;
              // Pass 1
              let lines = dbus_pcap_out1.split('\n');
              timestamps1 = [];
              for (let i = 0; i < lines.length; i++) {
                const l = lines[i].trim();
                try {
                  if (l.length > 0) {
                    const l0 = l.substr(0, l.indexOf(':'));
                    const ts_usec = parseFloat(l0) * 1000.0;
                    if (!isNaN(ts_usec)) {
                      timestamps1.push(ts_usec)
                    } else {
                      console.log('NaN! ' + l)
                    }
                  }
                } catch (x) {
                  console.log(x)
                }
              }
              Timestamps_DBus = timestamps1;

              // Pass 2
              temp1 = [];
              lines = dbus_pcap_out2.split('\n');
              for (let i = 0; i < lines.length; i++) {
                try {
                  temp1.push(JSON.parse(lines[i]));
                } catch (x) {
                  console.log('Line ' + i + ' could not be parsed');
                }
              }
              Data_DBus = temp1.slice();

              ShowBlocker('Processing dbus dump');
              OnGroupByConditionChanged_DBus();
              const v = dbus_timeline_view;
              let preproc = Preprocess_DBusPcap(temp1, timestamps1);
              let grouped = Group_DBus(preproc, v.GroupBy);
              GenerateTimeLine_DBus(grouped);
              dbus_timeline_view.IsCanvasDirty = true;
              if (dbus_timeline_view.IsEmpty() == false ||
                  ipmi_timeline_view.IsEmpty() == false) {
                dbus_timeline_view.CurrentFileName = x;
                ipmi_timeline_view.CurrentFileName = x;
                HideWelcomeScreen();
                ShowDBusTimeline();
                ShowIPMITimeline();
                ShowNavigation();
                UpdateFileNamesString();
              }
              HideBlocker();
            });
      });

  if (false) {
    document.getElementById('file_name').textContent = x;
    fs.readFile(x, {encoding: 'utf-8'}, (err, data) => {
      if (err) {
        console.log('Error in readFile: ' + err);
      } else {
        ParseIPMIDump(data);
      }
    });
  }
}

// The file may be either DBus dump or Boost Asio handler log
document.getElementById('btn_open_file')
    .addEventListener('click', OpenFileHandler);
document.getElementById('btn_open_file2')
    .addEventListener('click', OpenFileHandler);

document.getElementById('bah1').addEventListener(
    'click', OnGroupByConditionChanged_ASIO);
document.getElementById('bah2').addEventListener(
    'click', OnGroupByConditionChanged_ASIO);
document.getElementById('bah3').addEventListener(
    'click', OnGroupByConditionChanged_ASIO);

// UI elements
var g_group_by_dbus = document.getElementById('span_group_by_dbus');
var g_group_by_ipmi = document.getElementById('span_group_by_ipmi');
var g_group_by_asio =
    document.getElementById('span_group_by_boost_asio_handler');
var g_canvas_ipmi = document.getElementById('my_canvas_ipmi');
var g_canvas_dbus = document.getElementById('my_canvas_dbus');
var g_canvas_asio = document.getElementById('my_canvas_boost_asio_handler');

function ShowDBusTimeline() {
  g_canvas_dbus.style.display = 'block';
  g_group_by_dbus.style.display = 'block';
}
function ShowIPMITimeline() {
  g_canvas_ipmi.style.display = 'block';
  g_group_by_ipmi.style.display = 'block';
}
function ShowASIOTimeline() {
  g_canvas_asio.style.display = 'block';
  g_group_by_asio.style.display = 'block';
}

function Init() {
  console.log('[Init] Initialization');
  ipmi_timeline_view.Canvas = document.getElementById('my_canvas_ipmi');
  dbus_timeline_view.Canvas = document.getElementById('my_canvas_dbus');
  boost_asio_handler_timeline_view.Canvas =
      document.getElementById('my_canvas_boost_asio_handler');

  // Hide all canvases until the user loads files
  ipmi_timeline_view.Canvas.style.display = 'none';
  dbus_timeline_view.Canvas.style.display = 'none';
  boost_asio_handler_timeline_view.Canvas.style.display = 'none';

  let v0 = ipmi_timeline_view;
  let v1 = dbus_timeline_view;
  let v2 = boost_asio_handler_timeline_view;

  // Link views
  v0.linked_views = [v1, v2];
  v1.linked_views = [v0, v2];
  v2.linked_views = [v0, v1];

  // Set accent color
  v0.AccentColor = '#0CC';
  v1.AccentColor = '#080';
  v2.AccentColor = '#E22';
}

var g_WelcomeScreen = document.getElementById('welcome_screen');
function HideWelcomeScreen() {
  g_WelcomeScreen.style.display = 'none';
}

var g_Blocker = document.getElementById('blocker');
var g_BlockerCaption = document.getElementById('blocker_caption');
function HideBlocker() {
  g_Blocker.style.display = 'none';
}
function ShowBlocker(msg) {
  g_Blocker.style.display = 'block';
  g_BlockerCaption.textContent = msg;
}

var g_Navigation = document.getElementById('div_navi_and_replay');
function ShowNavigation() {
  g_Navigation.style.display = 'block';
}

function UpdateFileNamesString() {
  let tmp = [];
  if (ipmi_timeline_view.CurrentFileName != '') {
    tmp.push('IPMI timeline: ' + ipmi_timeline_view.CurrentFileName)
  }
  if (dbus_timeline_view.CurrentFileName != '') {
    tmp.push('DBus timeline: ' + dbus_timeline_view.CurrentFileName)
  }
  if (boost_asio_handler_timeline_view.CurrentFileName != '') {
    tmp.push(
        'ASIO timeline: ' + boost_asio_handler_timeline_view.CurrentFileName);
  }
  let s = tmp.join('<br/>');
  document.getElementById('capture_info').innerHTML = s;
}

Init();

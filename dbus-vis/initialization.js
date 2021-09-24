const { spawnSync } = require('child_process');
const md5File = require('md5-file');
const https = require('https');

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
    HideBlocker();
    return;
  }

  OpenDBusPcapFile(x);

  UpdateLayout();
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
    document.getElementById('span_group_by_boost_asio_handler')
var g_canvas_ipmi = document.getElementById('my_canvas_ipmi');
var g_canvas_dbus = document.getElementById('my_canvas_dbus');
var g_canvas_asio = document.getElementById('my_canvas_boost_asio_handler');

var g_dbus_pcap_status_content = document.getElementById('dbus_pcap_status_content');
var g_dbus_pcap_error_content = document.getElementById('dbus_pcap_error_content');
var g_btn_download_dbus_pcap = document.getElementById('btn_download_dbus_pcap');
var g_welcome_screen_content = document.getElementById('welcome_screen_content');
var g_scapy_error_content = document.getElementById('scapy_error_content');

function DownloadDbusPcap() {
  const url = 'https://raw.githubusercontent.com/openbmc/openbmc-tools/08ce0a5bad2b5c970af567c2e9888d444afe3946/dbus-pcap/dbus-pcap';

  https.get(url, (res) => {
    const path = 'dbus-pcap';
    const file_path = fs.createWriteStream(path);
    res.pipe(file_path);
    file_path.on('finish', () => {
      file_path.close();
      alert("dbus-pcap download complete!");
      CheckDbusPcapPresence();
    });
  });
}

g_btn_download_dbus_pcap.addEventListener(
  'click', DownloadDbusPcap);

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

// Make sure the user has scapy.all installed
function IsPythonInstallationOK() {
  const x = spawnSync('python3', ['-m', 'scapy.all']);
  return (x.status == 0);
}

function IsDbusPcapPresent() {
  if (fs.existsSync('dbus-pcap')) {
    return true;
  } else {
    return false;
  }
}

function CheckDependencies() {
  g_dbus_pcap_status_content.style.display = 'none';
  g_dbus_pcap_error_content.style.display = 'none';
  g_scapy_error_content.style.display = 'none';
  g_welcome_screen_content.style.display = 'none';

  const dbus_pcap_ok = IsDbusPcapPresent();
  if (!dbus_pcap_ok) {
    g_dbus_pcap_error_content.style.display = 'block';
  }

  const scapy_ok = IsPythonInstallationOK();
  if (!scapy_ok) {
    g_scapy_error_content.style.display = 'block';
  }

  let msg = "";
  if (dbus_pcap_ok) {
    msg += 'dbus-pcap found, md5sum: ' +
      md5File.sync('dbus-pcap');
    g_dbus_pcap_status_content.style.display = 'block';
    g_dbus_pcap_status_content.textContent = msg;
  }

  if (dbus_pcap_ok && scapy_ok) {
    g_welcome_screen_content.style.display = 'block';
  }
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

  let v1 = ipmi_timeline_view;
  let v2 = dbus_timeline_view;
  let v3 = boost_asio_handler_timeline_view;

  // Link views
  v1.linked_views = [v2, v3];
  v2.linked_views = [v1, v3];
  v3.linked_views = [v1, v2];

  // Set accent color
  v1.AccentColor = 'rgba(0,224,224,0.5)';
  v2.AccentColor = 'rgba(0,128,0,  0.5)';
  v3.AccentColor = '#E22';

  CheckDependencies();
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

var g_cb_debug_info = document.getElementById('cb_debuginfo');


Init();

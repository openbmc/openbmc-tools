// All of the Node.js APIs are available in the preload process.
// It has the same sandbox as a Chrome extension.

const dialog = require('electron').remote.dialog;  // Must add "remote"

window.addEventListener('DOMContentLoaded', () => {
  const replaceText = (selector, text) => {
    const element = document.getElementById(selector);
    if (element) element.innerText = text;
  };

  for (const type of ['chrome', 'node', 'electron']) {
    replaceText(`${type}-version`, process.versions[type]);
  }
})

console.log(document.getElementById('btn_open_file'));

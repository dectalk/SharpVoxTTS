import SharpVoxModule from './sharpvox.js';

const Module = await SharpVoxModule();
const instance = new Module.SharpVoxInterop();
window.sharpVox = instance;
instance.Initialize();

const activeTab = document.querySelector('.tab-btn.active');
if (activeTab) {
    const mode = activeTab.id.replace('tab-', '');
    instance.SetMode(mode === 'klattsch' || mode === 'ust');
}

window.dispatchEvent(new Event('sharpvox-ready'));

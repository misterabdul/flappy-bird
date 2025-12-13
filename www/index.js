const wasmContainer = document.getElementById("wasm-container");
wasmContainer.addEventListener(
    "webglcontextlost",
    (event) => {
        alert("WebGL context lost. You will need to reload the page.");
        event.preventDefault();
    },
    false
);

const wasmRepo = document.getElementById("wasm-repo");
wasmRepo.addEventListener("click", () => {
    window.open("https://github.com/misterabdul/flappy-bird", "_blank");
});

const wasmFullscreenToggle = document.getElementById("wasm-fullscreen");
wasmFullscreenToggle.addEventListener("click", () => {
    if (!document.fullscreenElement) {
        wasmContainer.requestFullscreen();
    } else {
        document.exitFullscreen();
    }
});

var Module = {
    print: (text) => console.log(text),
    printErr: (text) => console.error(text),
    canvas: wasmContainer,
};

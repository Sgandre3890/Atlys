const canvas = document.getElementById("glcanvas");

// Request anti-aliasing here
const gl = canvas.getContext("webgl", {
    antialias: true,
    depth: true,
    stencil: false
});

if (!gl) {
    alert("WebGL not supported");
}

// ----------------------------
// WebGL State Setup
// ----------------------------

// Enable depth testing
gl.enable(gl.DEPTH_TEST);
gl.depthFunc(gl.LEQUAL);

// Enable back-face culling
gl.enable(gl.CULL_FACE);
gl.cullFace(gl.BACK);

// Set viewport
gl.viewport(0, 0, canvas.width, canvas.height);

// Clear color
gl.clearColor(0.1, 0.1, 0.1, 1.0);

// ----------------------------
// Shader Source
// ----------------------------

const vsSource = `
attribute vec3 aPos;

void main() {
    gl_Position = vec4(aPos, 1.0);
}
`;

const fsSource = `
precision mediump float;

void main() {
    gl_FragColor = vec4(1.0, 0.4, 0.2, 1.0);
}
`;

// ----------------------------
// Shader Compilation
// ----------------------------

function compileShader(type, source) {
    const shader = gl.createShader(type);
    gl.shaderSource(shader, source);
    gl.compileShader(shader);

    if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS)) {
        console.error("Shader error:", gl.getShaderInfoLog(shader));
        gl.deleteShader(shader);
        return null;
    }
    return shader;
}

const vertexShader = compileShader(gl.VERTEX_SHADER, vsSource);
const fragmentShader = compileShader(gl.FRAGMENT_SHADER, fsSource);

// Link program
const program = gl.createProgram();
gl.attachShader(program, vertexShader);
gl.attachShader(program, fragmentShader);
gl.linkProgram(program);

if (!gl.getProgramParameter(program, gl.LINK_STATUS)) {
    console.error("Program error:", gl.getProgramInfoLog(program));
}

gl.useProgram(program);

// Geometry 

const vertices = new Float32Array([
    0.0,  0.5, 0.0,
   -0.5, -0.5, 0.0,
    0.5, -0.5, 0.0
]);

const vbo = gl.createBuffer();
gl.bindBuffer(gl.ARRAY_BUFFER, vbo);
gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);

const aPos = gl.getAttribLocation(program, "aPos");
gl.enableVertexAttribArray(aPos);
gl.vertexAttribPointer(aPos, 3, gl.FLOAT, false, 0, 0);

function resizeCanvasToWidescreen(canvas) {
    const aspect = 4 / 3;

    let width = window.innerWidth;
    let height = window.innerHeight;

    if (width / height > aspect) {
        width = height * aspect;
    } else {
        height = width / aspect;
    }

    canvas.width = width;
    canvas.height = height;

    gl.viewport(0, 0, canvas.width, canvas.height);
}

// Render Loop

function render() {
    resizeCanvasToWidescreen(canvas);

    gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);
    gl.drawArrays(gl.TRIANGLES, 0, 3);

    requestAnimationFrame(render);
}

render();

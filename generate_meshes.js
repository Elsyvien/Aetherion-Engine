const fs = require('fs');
const path = require('path');

const outputDir = path.join(__dirname, 'build-mingw', 'assets', 'meshes');

function createGltf(name, positions, indices, colors) {
    // Pack positions (float32 x 3)
    const posBuffer = Buffer.alloc(positions.length * 3 * 4);
    for (let i = 0; i < positions.length; i++) {
        posBuffer.writeFloatLE(positions[i][0], i * 12);
        posBuffer.writeFloatLE(positions[i][1], i * 12 + 4);
        posBuffer.writeFloatLE(positions[i][2], i * 12 + 8);
    }
    
    // Pack normals - compute from positions (simplified, pointing outward)
    const normBuffer = Buffer.alloc(positions.length * 3 * 4);
    for (let i = 0; i < positions.length; i++) {
        const p = positions[i];
        const len = Math.sqrt(p[0]*p[0] + p[1]*p[1] + p[2]*p[2]) || 1;
        normBuffer.writeFloatLE(p[0]/len, i * 12);
        normBuffer.writeFloatLE(p[1]/len, i * 12 + 4);
        normBuffer.writeFloatLE(p[2]/len, i * 12 + 8);
    }
    
    // Pack colors (float32 x 4)
    const colBuffer = Buffer.alloc(positions.length * 4 * 4);
    for (let i = 0; i < positions.length; i++) {
        const c = colors[i] || [1, 1, 1, 1];
        colBuffer.writeFloatLE(c[0], i * 16);
        colBuffer.writeFloatLE(c[1], i * 16 + 4);
        colBuffer.writeFloatLE(c[2], i * 16 + 8);
        colBuffer.writeFloatLE(c[3], i * 16 + 12);
    }
    
    // Pack indices (uint16)
    const idxBuffer = Buffer.alloc(indices.length * 2);
    for (let i = 0; i < indices.length; i++) {
        idxBuffer.writeUInt16LE(indices[i], i * 2);
    }
    
    const binary = Buffer.concat([posBuffer, normBuffer, colBuffer, idxBuffer]);
    const b64 = binary.toString('base64');
    
    // Calculate bounds
    let minX = Infinity, minY = Infinity, minZ = Infinity;
    let maxX = -Infinity, maxY = -Infinity, maxZ = -Infinity;
    for (const p of positions) {
        minX = Math.min(minX, p[0]); maxX = Math.max(maxX, p[0]);
        minY = Math.min(minY, p[1]); maxY = Math.max(maxY, p[1]);
        minZ = Math.min(minZ, p[2]); maxZ = Math.max(maxZ, p[2]);
    }
    
    const gltf = {
        asset: { generator: "Aetherion Engine", version: "2.0" },
        scene: 0,
        scenes: [{ nodes: [0] }],
        nodes: [{ mesh: 0, name: name }],
        meshes: [{ name: name, primitives: [{ 
            attributes: { POSITION: 0, NORMAL: 1, COLOR_0: 2 }, 
            indices: 3 
        }]}],
        accessors: [
            { bufferView: 0, componentType: 5126, count: positions.length, type: "VEC3", 
              max: [maxX, maxY, maxZ], min: [minX, minY, minZ] },
            { bufferView: 1, componentType: 5126, count: positions.length, type: "VEC3" },
            { bufferView: 2, componentType: 5126, count: positions.length, type: "VEC4" },
            { bufferView: 3, componentType: 5123, count: indices.length, type: "SCALAR" }
        ],
        bufferViews: [
            { buffer: 0, byteOffset: 0, byteLength: posBuffer.length },
            { buffer: 0, byteOffset: posBuffer.length, byteLength: normBuffer.length },
            { buffer: 0, byteOffset: posBuffer.length + normBuffer.length, byteLength: colBuffer.length },
            { buffer: 0, byteOffset: posBuffer.length + normBuffer.length + colBuffer.length, byteLength: idxBuffer.length }
        ],
        buffers: [{ uri: `data:application/octet-stream;base64,${b64}`, byteLength: binary.length }]
    };
    
    const filePath = path.join(outputDir, `${name}.gltf`);
    fs.writeFileSync(filePath, JSON.stringify(gltf, null, 2));
    console.log(`Created ${name}.gltf (${positions.length} vertices, ${indices.length} indices)`);
}

// Diamond (Octahedron) - flat shaded (duplicate vertices per face)
// This avoids smooth interpolation across faces, making the shape read as 3D.
const diamondTop = [0, 0.8, 0];
const diamondBottom = [0, -0.8, 0];
const diamondRing = [
    [0.55, 0, 0],   // +X
    [0, 0, 0.55],   // +Z
    [-0.55, 0, 0],  // -X
    [0, 0, -0.55],  // -Z
];

// Faces as triangles referencing the conceptual points above
const topFaces = [
    [diamondTop, diamondRing[0], diamondRing[1]],
    [diamondTop, diamondRing[1], diamondRing[2]],
    [diamondTop, diamondRing[2], diamondRing[3]],
    [diamondTop, diamondRing[3], diamondRing[0]],
];
const bottomFaces = [
    [diamondBottom, diamondRing[1], diamondRing[0]],
    [diamondBottom, diamondRing[2], diamondRing[1]],
    [diamondBottom, diamondRing[3], diamondRing[2]],
    [diamondBottom, diamondRing[0], diamondRing[3]],
];

const faceColors = [
    [1, 0.2, 0.2, 1],
    [0.2, 1, 0.2, 1],
    [0.2, 0.4, 1, 1],
    [1, 1, 0.2, 1],
    [1, 0.2, 1, 1],
    [0.2, 1, 1, 1],
    [1, 0.6, 0.2, 1],
    [0.7, 0.3, 1, 1],
];

const diamondPos = [];
const diamondIdx = [];
const diamondCol = [];

const allFaces = [...topFaces, ...bottomFaces];
for (let f = 0; f < allFaces.length; f++) {
    const tri = allFaces[f];
    const base = diamondPos.length;
    diamondPos.push(tri[0], tri[1], tri[2]);
    diamondIdx.push(base, base + 1, base + 2);
    const c = faceColors[f] || [1, 1, 1, 1];
    diamondCol.push(c, c, c);
}

createGltf('Diamond', diamondPos, diamondIdx, diamondCol);

// Cube (8 vertices, 12 triangles = 36 indices)
const s = 0.4;
const cubePos = [
    [-s, -s, -s], [s, -s, -s], [s, s, -s], [-s, s, -s],  // Back face
    [-s, -s, s], [s, -s, s], [s, s, s], [-s, s, s]       // Front face
];
const cubeIdx = [
    0, 2, 1, 0, 3, 2,  // Back
    4, 5, 6, 4, 6, 7,  // Front
    0, 1, 5, 0, 5, 4,  // Bottom
    2, 3, 7, 2, 7, 6,  // Top
    0, 4, 7, 0, 7, 3,  // Left
    1, 2, 6, 1, 6, 5   // Right
];
const cubeCol = [
    [1, 0, 0, 1], [0, 1, 0, 1], [0, 0, 1, 1], [1, 1, 0, 1],
    [1, 0, 1, 1], [0, 1, 1, 1], [1, 0.5, 0, 1], [0.5, 0, 1, 1]
];
createGltf('Cube', cubePos, cubeIdx, cubeCol);

// Pyramid (5 vertices)
const pyPos = [
    [0, 0.6, 0],       // 0: Top
    [0.5, -0.3, 0.5],  // 1: Front-Right
    [-0.5, -0.3, 0.5], // 2: Front-Left
    [-0.5, -0.3, -0.5],// 3: Back-Left
    [0.5, -0.3, -0.5]  // 4: Back-Right
];
const pyIdx = [
    0, 1, 2,  0, 2, 3,  0, 3, 4,  0, 4, 1,  // 4 side faces
    1, 3, 2,  1, 4, 3   // Bottom (2 triangles)
];
const pyCol = [
    [1, 1, 0, 1],      // Yellow top
    [1, 0.5, 0, 1],    // Orange
    [0, 1, 0, 1],      // Green
    [0, 0.5, 1, 1],    // Blue
    [1, 0, 0.5, 1]     // Pink
];
createGltf('Pyramid', pyPos, pyIdx, pyCol);

// Tetrahedron (4 vertices, 4 triangular faces)
const t = 0.5;
const tetraPos = [
    [0, 0.6, 0],           // Top
    [t, -0.3, t],          // Front-Right
    [-t, -0.3, t],         // Front-Left  
    [0, -0.3, -t*1.2]      // Back
];
const tetraIdx = [
    0, 1, 2,   // Front
    0, 2, 3,   // Left
    0, 3, 1,   // Right
    1, 3, 2    // Bottom
];
const tetraCol = [
    [1, 0, 0, 1],   // Red
    [0, 1, 0, 1],   // Green
    [0, 0, 1, 1],   // Blue
    [1, 1, 0, 1]    // Yellow
];
createGltf('Tetrahedron', tetraPos, tetraIdx, tetraCol);

// Sphere approximation (Icosahedron - 12 vertices, 20 faces)
const phi = (1 + Math.sqrt(5)) / 2;
const a = 0.3;
const b = a * phi;
const icoPos = [
    [-a, b, 0], [a, b, 0], [-a, -b, 0], [a, -b, 0],
    [0, -a, b], [0, a, b], [0, -a, -b], [0, a, -b],
    [b, 0, -a], [b, 0, a], [-b, 0, -a], [-b, 0, a]
];
const icoIdx = [
    0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11,
    1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
    3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9,
    4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1
];
const icoCol = icoPos.map((p, i) => {
    const hue = i / 12;
    return [
        Math.sin(hue * Math.PI * 2) * 0.5 + 0.5,
        Math.sin(hue * Math.PI * 2 + 2) * 0.5 + 0.5,
        Math.sin(hue * Math.PI * 2 + 4) * 0.5 + 0.5,
        1
    ];
});
createGltf('Icosphere', icoPos, icoIdx, icoCol);

console.log('Done! All meshes created.');

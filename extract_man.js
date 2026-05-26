// Extract only "Man (Rig)" from HumanModels.glb
// Uses raw GLB manipulation

const fs = require('fs');
const path = require('path');

// Read GLB
const buf = fs.readFileSync(path.join(__dirname, 'source/HumanModels.glb'));

// Parse GLB header
const magic = buf.readUInt32LE(0);
const version = buf.readUInt32LE(4);
const totalLength = buf.readUInt32LE(8);

// Chunk 0: JSON
const jsonChunkLen = buf.readUInt32LE(12);
const jsonChunkType = buf.readUInt32LE(16);
const jsonStr = buf.slice(20, 20 + jsonChunkLen).toString();
const gltf = JSON.parse(jsonStr);

// Chunk 1: Binary
const binOffset = 20 + jsonChunkLen;
const binChunkLen = buf.readUInt32LE(binOffset);
const binChunkType = buf.readUInt32LE(binOffset + 4);
const binData = buf.slice(binOffset + 8, binOffset + 8 + binChunkLen);

// Scene root nodes: [153, 154, 308, 309, 310, 311]
// Keep only node 153 (Man Rig) 
// Node 153: Man (Rig) - has skeleton with mesh
gltf.scenes[0].nodes = [153];

// Fix Man (Rig) position to center
gltf.nodes[153].translation = [0, 0, 0];

// Write new GLB
const newJsonStr = JSON.stringify(gltf);
const jsonBuf = Buffer.from(newJsonStr);
// Pad to 4-byte alignment
const jsonPadLen = (4 - (jsonBuf.length % 4)) % 4;
const paddedJsonBuf = Buffer.concat([jsonBuf, Buffer.alloc(jsonPadLen, 0x20)]);

// Build GLB
const header = Buffer.alloc(12);
header.writeUInt32LE(0x46546C67, 0); // magic: glTF
header.writeUInt32LE(2, 4); // version
header.writeUInt32LE(12 + 8 + paddedJsonBuf.length + 8 + binData.length, 8); // total length

const jsonChunkHeader = Buffer.alloc(8);
jsonChunkHeader.writeUInt32LE(paddedJsonBuf.length, 0);
jsonChunkHeader.writeUInt32LE(0x4E4F534A, 4); // JSON

const binChunkHeader = Buffer.alloc(8);
binChunkHeader.writeUInt32LE(binData.length, 0);
binChunkHeader.writeUInt32LE(0x004E4942, 4); // BIN

const output = Buffer.concat([header, jsonChunkHeader, paddedJsonBuf, binChunkHeader, binData]);
fs.writeFileSync(path.join(__dirname, 'dashboard/ManModel.glb'), output);

console.log('Done! ManModel.glb created (' + (output.length / 1024 / 1024).toFixed(2) + ' MB)');
console.log('Contains only: Man (Rig) centered at origin');

// Extract Man (Rig) + Man (HighPoly) only
// Man (Rig) = node 153 (has skeleton for posing)
// Man (HighPoly) = node 311 (looks good but no rig)
// We keep Man (Rig) for bone control, hide its low-poly mesh in JS

const fs = require('fs');
const path = require('path');

const buf = fs.readFileSync(path.join(__dirname, 'source/HumanModels.glb'));
const jsonChunkLen = buf.readUInt32LE(12);
const jsonStr = buf.slice(20, 20 + jsonChunkLen).toString();
const gltf = JSON.parse(jsonStr);

const binOffset = 20 + jsonChunkLen;
const binChunkLen = buf.readUInt32LE(binOffset);
const binData = buf.slice(binOffset + 8, binOffset + 8 + binChunkLen);

// Keep only Man (Rig) node 153 — it has the skeleton
// Center it at origin
gltf.scenes[0].nodes = [153];
gltf.nodes[153].translation = [0, 0, 0];

// Write GLB
const newJsonStr = JSON.stringify(gltf);
const jsonBuf = Buffer.from(newJsonStr);
const jsonPadLen = (4 - (jsonBuf.length % 4)) % 4;
const paddedJsonBuf = Buffer.concat([jsonBuf, Buffer.alloc(jsonPadLen, 0x20)]);

const header = Buffer.alloc(12);
header.writeUInt32LE(0x46546C67, 0);
header.writeUInt32LE(2, 4);
header.writeUInt32LE(12 + 8 + paddedJsonBuf.length + 8 + binData.length, 8);

const jsonChunkHeader = Buffer.alloc(8);
jsonChunkHeader.writeUInt32LE(paddedJsonBuf.length, 0);
jsonChunkHeader.writeUInt32LE(0x4E4F534A, 4);

const binChunkHeader = Buffer.alloc(8);
binChunkHeader.writeUInt32LE(binData.length, 0);
binChunkHeader.writeUInt32LE(0x004E4942, 4);

const output = Buffer.concat([header, jsonChunkHeader, paddedJsonBuf, binChunkHeader, binData]);
fs.writeFileSync(path.join(__dirname, 'dashboard/Man.glb'), output);
console.log('Man.glb created (' + (output.length / 1024 / 1024).toFixed(2) + ' MB) - Man (Rig) only, centered');

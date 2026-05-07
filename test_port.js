import net from 'net';

const client = new net.Socket();
const HOST = '8.134.167.240';
const PORT = 1883;

console.log(`Testing connection to ${HOST}:${PORT}...`);

client.setTimeout(5000);

client.connect(PORT, HOST, () => {
  console.log('✅ SUCCESS: Port 1883 is OPEN and reachable!');
  client.destroy();
  process.exit(0);
});

client.on('error', (err) => {
  console.error('❌ FAILED: Connection error:', err.message);
  process.exit(1);
});

client.on('timeout', () => {
  console.error('❌ FAILED: Connection timed out.');
  console.error('   Possible reasons:');
  console.error('   1. Aliyun Security Group does not allow port 1883.');
  console.error('   2. Mosquitto service is not running on the server.');
  client.destroy();
  process.exit(1);
});

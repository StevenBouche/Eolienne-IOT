import { createServer } from 'http';
import app from "./app.js";

var port = 8080;

createServer(app).listen(port, () => {
    console.log("Listening at :" + port + "...");
});
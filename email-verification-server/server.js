const express = require('express');
const nodemailer = require('nodemailer');
const cors = require('cors');

const app = express();
app.use(cors());
app.use(express.json());

app.post('/send-verification-code', (req, res) => {
    const { email, code } = req.body;

    const transporter = nodemailer.createTransport({
        service: 'gmail',
        auth: {
            user: 'tandangvo01297155542@gmail.com',
            pass: 'unww qvev ozbc ebgg' // Mật khẩu ứng dụng Gmail
        }
    });

    const mailOptions = {
        from: 'tandangvo01297155542@gmail.com',
        to: email,
        subject: 'Mã xác nhận đăng ký',
        text: `Mã xác nhận của bạn là: ${code}`
    };

    transporter.sendMail(mailOptions, (error, info) => {
        if (error) {
            console.error('Lỗi gửi email:', error);
            return res.status(500).json({ success: false, message: 'Lỗi khi gửi email' });
        }
        console.log('Đã gửi email:', info.response);
        res.status(200).json({ success: true, message: 'Email đã được gửi thành công' });
    });
});

const PORT = 3000;
app.listen(PORT, () => console.log(`Server đang chạy tại http://localhost:${PORT}`));

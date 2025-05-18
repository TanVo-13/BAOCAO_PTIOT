import firebase from './firebase.js';

// Hiển thị/ẩn modal
const loginModal = document.getElementById("login-modal");
const signupModal = document.getElementById("signup-modal");

document.getElementById("login-btn").onclick = () =>
  (loginModal.style.display = "block");
document.getElementById("signup-btn").onclick = () =>
  (signupModal.style.display = "block");

document.querySelectorAll(".close").forEach((btn) => {
  btn.onclick = () => {
    loginModal.style.display = "none";
    signupModal.style.display = "none";
  };
});

window.onclick = (event) => {
  if (event.target == loginModal) loginModal.style.display = "none";
  if (event.target == signupModal) signupModal.style.display = "none";
};

// Hàm đặt lại dữ liệu cảm biến về "Chưa xác định"
function resetSensorData() {
  document.getElementById("humidity").innerText = "Chưa xác định";
  document.getElementById("temperature").innerText = "Chưa xác định";
  document.getElementById("humidity-soil").innerText = "Chưa xác định";
  document.getElementById("soil-status").innerText = "Chưa xác định";
  document.getElementById("gas-status").innerText = "Chưa xác định";
  document.getElementById("gas-value").innerText = "Chưa xác định";
  document.getElementById("rain-status").innerText = "Chưa xác định";
  document.getElementById("water-level").innerText = "Chưa xác định";
  document.getElementById("water-status").innerText = "Chưa xác định";
  document.getElementById("light-intensity").innerText = "Chưa xác định";
  document.getElementById("light-level").innerText = "Chưa xác định";
}

// Kiểm tra trạng thái đăng nhập khi tải trang
firebase.auth().onAuthStateChanged((user) => {
  const userInfo = document.getElementById("user-info");
  const userName = document.getElementById("user-name");
  const loginBtn = document.getElementById("login-btn");
  const signupBtn = document.getElementById("signup-btn");
  const logoutBtn = document.getElementById("logout-btn");

  if (user) {
    userName.textContent = user.displayName || user.email;
    userInfo.style.display = "inline-block";
    logoutBtn.style.display = "inline-block";
    loginBtn.style.display = "none";
    signupBtn.style.display = "none";
  } else {
    userInfo.style.display = "none";
    logoutBtn.style.display = "none";
    loginBtn.style.display = "inline-block";
    signupBtn.style.display = "inline-block";
    resetSensorData(); // Đặt lại dữ liệu cảm biến khi đăng xuất
  }
});

// Đăng ký
let generatedCode = null;

document.getElementById("send-code-btn").addEventListener("click", function () {
  const email = document.getElementById("signup-email").value;
  if (!email) {
    alert("Vui lòng nhập email trước khi gửi mã.");
    return;
  }

  generatedCode = Math.floor(100000 + Math.random() * 900000);

  fetch("http://localhost:3000/send-verification-code", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ email: email, code: generatedCode }),
  })
    .then((res) => res.json())
    .then((data) => {
      if (data.success) {
        alert("Đã gửi mã xác nhận đến email!");
      } else {
        alert("Không thể gửi mã. Vui lòng thử lại.");
      }
    })
    .catch((err) => {
      console.error(err);
      alert("Lỗi khi gửi mã xác nhận.");
    });
});

document.getElementById("signup-form").addEventListener("submit", function (e) {
  e.preventDefault();
  const name = document.getElementById("signup-name").value;
  const email = document.getElementById("signup-email").value;
  const password = document.getElementById("signup-password").value;
  const inputCode = document.getElementById("verification-code").value;

  if (!generatedCode || inputCode != generatedCode) {
    alert("Mã xác nhận không đúng hoặc chưa được gửi.");
    return;
  }

  firebase
    .auth()
    .fetchSignInMethodsForEmail(email)
    .then((methods) => {
      if (methods.length > 0) {
        alert("Email đã tồn tại. Vui lòng đăng nhập hoặc dùng email khác.");
        throw new Error("Email đã tồn tại");
      } else {
        return firebase.auth().createUserWithEmailAndPassword(email, password);
      }
    })
    .then((userCredential) => {
      if (userCredential) {
        return userCredential.user.updateProfile({
          displayName: name,
        });
      }
    })
    .then(() => {
      alert("Đăng ký thành công!");
      signupModal.style.display = "none";
      generatedCode = null;
    })
    .catch((error) => {
      if (error.message !== "Email đã tồn tại") {
        alert("Lỗi: " + error.message);
      }
    });
});

// Đăng nhập
document.getElementById("login-form").addEventListener("submit", function (e) {
  e.preventDefault();
  const email = document.getElementById("login-email").value;
  const password = document.getElementById("login-password").value;

  firebase
    .auth()
    .signInWithEmailAndPassword(email, password)
    .then((userCredential) => {
      alert("Đăng nhập thành công!");
      loginModal.style.display = "none";
    })
    .catch((error) => {
      alert("Lỗi: " + error.message);
    });
});

// Đăng xuất
document.getElementById("logout-btn").addEventListener("click", function () {
  firebase
    .auth()
    .signOut()
    .then(() => {
      alert("Đã đăng xuất!");
    })
    .catch((error) => {
      alert("Lỗi: " + error.message);
    });
});
const cells = document.querySelectorAll('.cell');
const statusText = document.getElementById('status');
const resetBtn = document.getElementById('reset');

let board = Array(9).fill(null);
let gameOver = false;

const winCombo = [
  [0,1,2], [3,4,5], [6,7,8],
  [0,3,6], [1,4,7], [2,5,8],
  [0,4,8], [2,4,6]
];

function checkWin() {
  for (let [a,b,c] of winCombo) {
    if (board[a] && board[a] === board[b] && board[a] === board[c]) {
      return board[a];
    }
  }
  if (board.every(v => v)) return 'draw';
  return null;
}

function playerMove(e) {
  if (gameOver) return;
  const index = e.target.dataset.index;
  if (board[index]) return;

  board[index] = 'X';
  e.target.textContent = 'X';
  e.target.classList.add('disabled');

  const result = checkWin();
  if (result) return endGame(result);

  setTimeout(computerMove, 300);
}

function computerMove() {
  if (gameOver) return;
  const empty = board.map((v, i) => v ? null : i).filter(v => v !== null);
  if (empty.length === 0) return;
  const pick = empty[Math.floor(Math.random() * empty.length)];
  board[pick] = 'O';
  cells[pick].textContent = 'O';
  cells[pick].classList.add('disabled');

  const result = checkWin();
  if (result) endGame(result);
}

function endGame(result) {
  gameOver = true;
  if (result === 'draw') statusText.textContent = 'Draw!';
  else statusText.textContent = 'winner: ' + result;
}

function resetGame() {
  board = Array(9).fill(null);
  gameOver = false;
  statusText.textContent = 'player: X';
  cells.forEach(c => {
    c.textContent = '';
    c.classList.remove('disabled');
  });
}

cells.forEach(c => c.addEventListener('click', playerMove));
resetBtn.addEventListener('click', resetGame);
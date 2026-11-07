const EXAMPLES = {
  arithmetic: `#include <iostream>

int main() {
    int a = 10;
    int b = 5;

    int sum = a + b;
    int product = a * b;

    std::cout << sum << std::endl;
    std::cout << product << std::endl;

    return 0;
}`,

  condition: `#include <iostream>

int main() {
    int score = 8;

    if (score >= 5) {
        std::cout << 1 << std::endl;
    } else {
        std::cout << 0 << std::endl;
    }

    return 0;
}`,

  loop: `#include <iostream>

int main() {
    int counter = 1;

    while (counter <= 5) {
        std::cout << counter << std::endl;
        counter = counter + 1;
    }

    return 0;
}`,

  division: `#include <iostream>

int main() {
    int a = -7;
    int b = 2;

    std::cout << a / b << std::endl;
    std::cout << a % b << std::endl;

    return 0;
}`
};

const sourceInput = document.getElementById('source');
const runButton = document.getElementById('run-button');
const examplesToggle = document.getElementById('examples-toggle');
const examplesMenu = document.getElementById('examples-menu');
const exampleButtons = document.querySelectorAll('[data-example]');
const outputElement = document.getElementById('output');
const generatedCodeElement = document.getElementById('generated-code');
const errorElement = document.getElementById('error');
const statusElement = document.getElementById('status');
const astContainer = document.getElementById('ast-container');
const astEmpty = document.getElementById('ast-empty');
const astSvg = document.getElementById('ast-svg');
const astCount = document.getElementById('ast-count');
const lineCount = document.getElementById('line-count');

sourceInput.value = EXAMPLES.arithmetic;
updateLineCount();

sourceInput.addEventListener('input', updateLineCount);
runButton.addEventListener('click', runAnalysis);
examplesToggle.addEventListener('click', toggleExamplesMenu);

for (const button of exampleButtons) {
  button.addEventListener('click', () => {
    const selectedExample = button.dataset.example;
    if (!Object.prototype.hasOwnProperty.call(EXAMPLES, selectedExample)) return;

    sourceInput.value = EXAMPLES[selectedExample];
    updateLineCount();
    closeExamplesMenu();
    sourceInput.focus();
  });
}

document.addEventListener('click', event => {
  if (!event.target.closest || !event.target.closest('.examples-picker')) closeExamplesMenu();
});

document.addEventListener('keydown', event => {
  if (event.key === 'Escape') closeExamplesMenu();
});

function toggleExamplesMenu() {
  const isOpen = examplesToggle.getAttribute('aria-expanded') === 'true';
  if (isOpen) {
    closeExamplesMenu();
  } else {
    examplesMenu.hidden = false;
    examplesToggle.setAttribute('aria-expanded', 'true');
  }
}

function closeExamplesMenu() {
  examplesMenu.hidden = true;
  examplesToggle.setAttribute('aria-expanded', 'false');
}

function updateLineCount() {
  const count = sourceInput.value.split('\n').length;
  lineCount.textContent = `${count} ${count === 1 ? 'рядок' : count < 5 ? 'рядки' : 'рядків'}`;
}

async function runAnalysis() {
  runButton.disabled = true;
  runButton.innerHTML = 'Виконується…';
  errorElement.hidden = true;
  errorElement.textContent = '';
  outputElement.textContent = 'Виконується аналіз…';
  generatedCodeElement.textContent = 'Генерується Python-код…';
  setStatus('Виконується', 'idle');

  try {
    const response = await fetch('/api/parse', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ source: sourceInput.value })
    });

    const result = await response.json();
    if (!response.ok && !result.error) {
      throw new Error(`HTTP ${response.status}`);
    }

    if (result.ast) {
      renderAst(result.ast);
    } else {
      clearAst();
    }

    generatedCodeElement.textContent = result.generatedCode
      || 'Python-код не згенеровано. Перевір синтаксис програми.';

    if (result.ok) {
      outputElement.textContent = result.output || '(Програма завершилася без виведення.)';
      setStatus('Успішно', 'success');
    } else {
      outputElement.textContent = 'Програму не виконано.';
      errorElement.textContent = result.error || 'Невідома помилка.';
      errorElement.hidden = false;
      setStatus('Помилка', 'error');
    }
  } catch (error) {
    outputElement.textContent = 'Не вдалося отримати результат від C++ backend.';
    errorElement.textContent = error.message;
    errorElement.hidden = false;
    setStatus('Помилка сервера', 'error');
    clearAst();
    generatedCodeElement.textContent = 'Не вдалося згенерувати Python-код.';
  } finally {
    runButton.disabled = false;
    runButton.innerHTML = '<span class="run-icon" aria-hidden="true">▶</span> Виконати';
  }
}

function setStatus(label, state) {
  statusElement.textContent = label;
  statusElement.className = `status status-${state}`;
}

function clearAst() {
  astSvg.replaceChildren();
  astContainer.hidden = true;
  astEmpty.hidden = false;
  astCount.textContent = '0 вузлів';
}

function nodeLabel(node) {
  return node.value ? `${node.kind} (${node.value})` : node.kind;
}

function nodeCategory(node) {
  if (['Program', 'MainFunction', 'Statements', 'Block'].includes(node.kind)) return 'syntax';
  if (['Number', 'Boolean', 'Identifier', 'Unary', 'Binary'].includes(node.kind)) return 'expression';
  return 'statement';
}

function renderAst(root) {
  const nodes = [];
  const edges = [];
  const horizontalGap = 25;
  const levelGap = 100;
  const boxHeight = 42;

  function measure(node) {
    const label = nodeLabel(node);
    node._boxWidth = Math.max(108, Math.min(218, label.length * 7.3 + 28));
    const childWidths = (node.children || []).map(measure);
    const childrenWidth = childWidths.reduce((sum, width) => sum + width, 0)
      + Math.max(0, childWidths.length - 1) * horizontalGap;
    node._treeWidth = Math.max(node._boxWidth, childrenWidth);
    return node._treeWidth;
  }

  function place(node, left, depth) {
    const x = left + node._treeWidth / 2;
    const y = 45 + depth * levelGap;
    nodes.push({ node, x, y });

    const children = node.children || [];
    const childrenWidth = children.reduce((sum, child) => sum + child._treeWidth, 0)
      + Math.max(0, children.length - 1) * horizontalGap;
    let childLeft = left + (node._treeWidth - childrenWidth) / 2;

    for (const child of children) {
      const childPosition = place(child, childLeft, depth + 1);
      edges.push({ x1: x, y1: y + boxHeight / 2, x2: childPosition.x, y2: childPosition.y - boxHeight / 2 });
      childLeft += child._treeWidth + horizontalGap;
    }
    return { x, y };
  }

  const treeWidth = measure(root);
  const maxDepth = getDepth(root);
  const svgWidth = Math.max(720, treeWidth + 80);
  const svgHeight = Math.max(190, 90 + (maxDepth - 1) * levelGap + boxHeight);
  place(root, (svgWidth - treeWidth) / 2, 0);

  astSvg.setAttribute('width', svgWidth);
  astSvg.setAttribute('height', svgHeight);
  astSvg.setAttribute('viewBox', `0 0 ${svgWidth} ${svgHeight}`);
  astSvg.replaceChildren();

  const edgeLayer = svgElement('g');
  for (const edge of edges) {
    const path = svgElement('path');
    const middleY = edge.y1 + (edge.y2 - edge.y1) / 2;
    path.setAttribute('d', `M ${edge.x1} ${edge.y1} C ${edge.x1} ${middleY}, ${edge.x2} ${middleY}, ${edge.x2} ${edge.y2}`);
    path.setAttribute('class', 'tree-edge');
    edgeLayer.appendChild(path);
  }
  astSvg.appendChild(edgeLayer);

  const nodeLayer = svgElement('g');
  for (const position of nodes) {
    const group = svgElement('g');
    group.setAttribute('transform', `translate(${position.x}, ${position.y})`);

    const rect = svgElement('rect');
    rect.setAttribute('x', -position.node._boxWidth / 2);
    rect.setAttribute('y', -boxHeight / 2);
    rect.setAttribute('width', position.node._boxWidth);
    rect.setAttribute('height', boxHeight);
    rect.setAttribute('rx', 9);
    rect.setAttribute('class', `tree-node node-${nodeCategory(position.node)}`);
    group.appendChild(rect);

    const text = svgElement('text');
    text.setAttribute('text-anchor', 'middle');
    text.setAttribute('dominant-baseline', 'middle');
    text.setAttribute('class', 'tree-label');
    text.textContent = shorten(nodeLabel(position.node), 27);
    group.appendChild(text);

    const title = svgElement('title');
    title.textContent = nodeLabel(position.node);
    group.appendChild(title);
    nodeLayer.appendChild(group);
  }
  astSvg.appendChild(nodeLayer);

  astContainer.hidden = false;
  astEmpty.hidden = true;
  astCount.textContent = `${nodes.length} ${nodes.length === 1 ? 'вузол' : nodes.length < 5 ? 'вузли' : 'вузлів'}`;
}

function getDepth(node) {
  if (!node.children || node.children.length === 0) return 1;
  return 1 + Math.max(...node.children.map(getDepth));
}

function shorten(text, maxLength) {
  return text.length > maxLength ? `${text.slice(0, maxLength - 1)}…` : text;
}

function svgElement(name) {
  return document.createElementNS('http://www.w3.org/2000/svg', name);
}

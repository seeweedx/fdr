#pragma once

// Red-Black Tree
//
// A self-balancing binary search tree where each node is colored
// RED or BLACK. The coloring rules guarantee that the longest path
// from root to any leaf is at most 2x the shortest, ensuring
// O(log n) operations even with adversarial insertion order.
//
// Red-Black properties:
//   1. Every node is either RED or BLACK
//   2. The root is always BLACK
//   3. Every null leaf is BLACK
//   4. If a node is RED, both its children must be BLACK
//   5. All paths from a node to its leaves contain the same
//      number of BLACK nodes (black-height)
//
// Operations:
//   insert   - O(log n) - BST insert + recolor/rotate to fix violations
//   contains - O(log n) - standard BST search
//   for_each - O(n)     - in-order traversal (sorted output)
//
// Used in fdr: stores deduplicated candidate line indices from
// trigram lookups. Duplicates are silently ignored on insert.

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace fdr {

template <typename T, typename Compare = std::less<T>>
class RBTree {
public:
  RBTree(Compare comp = Compare{}) : root_(nullptr), size_(0), comp_(comp) {}

  // Destructor: recursively free all nodes
  ~RBTree() { destroy(root_); }

  // Deep copy via recursive tree duplication
  RBTree(const RBTree& other) : root_(nullptr), size_(0), comp_(other.comp_) {
    root_ = copy_tree(other.root_, nullptr);
    size_ = other.size_;
  }

  RBTree& operator=(const RBTree& other) {
    if (this != &other) {
      destroy(root_);
      comp_ = other.comp_;
      root_ = copy_tree(other.root_, nullptr);
      size_ = other.size_;
    }
    return *this;
  }

  // Move: steal the other tree's root in O(1)
  RBTree(RBTree&& other) noexcept
    : root_(other.root_), size_(other.size_), comp_(std::move(other.comp_)) {
    other.root_ = nullptr;
    other.size_ = 0;
  }

  RBTree& operator=(RBTree&& other) noexcept {
    if (this != &other) {
      destroy(root_);
      root_ = other.root_;
      size_ = other.size_;
      comp_ = std::move(other.comp_);
      other.root_ = nullptr;
      other.size_ = 0;
    }
    return *this;
  }

  // Insert a value. Uses iterative BST insertion to find the correct
  // leaf position, then calls fix_insert to restore RB properties.
  // Duplicates are silently ignored (important for deduplicating
  // candidate indices from multiple trigram lookups).
  void insert(const T& value) {
    // Phase 1: Standard BST insert — walk down the tree to find
    // where the new node should go.
    Node* parent = nullptr;
    Node* current = root_;
    while (current) {
      parent = current;
      if (comp_(value, current->data))
        current = current->left;
      else if (comp_(current->data, value))
        current = current->right;
      else
        return; // Duplicate, do nothing
    }

    // Phase 2: Create new RED node and link it to its parent
    Node* n = new Node(value, parent);
    ++size_;

    if (!parent) {
      root_ = n; // Tree was empty
    } else if (comp_(value, parent->data)) {
      parent->left = n;
    } else {
      parent->right = n;
    }

    // Phase 3: Fix any red-black violations caused by the new RED node
    fix_insert(n);
    root_->color = Color::BLACK; // Property 2: root is always BLACK
  }

  // Standard BST search: O(log n)
  bool contains(const T& value) const {
    return find_node(root_, value) != nullptr;
  }

  // In-order traversal visits nodes in sorted order (left, root, right)
  template <typename Func>
  void for_each(Func func) const {
    in_order(root_, func);
  }

  // Collect all elements into a sorted vector via in-order traversal
  std::vector<T> to_vector() const {
    std::vector<T> result;
    result.reserve(size_);
    for_each([&result](const T& val) { result.push_back(val); });
    return result;
  }

  std::size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }

  void clear() {
    destroy(root_);
    root_ = nullptr;
    size_ = 0;
  }

private:
  enum class Color { RED, BLACK };

  struct Node {
    T data;
    Node* left;
    Node* right;
    Node* parent;
    Color color;

    // New nodes are always RED (least disruptive to black-height)
    Node(const T& d, Node* p = nullptr)
      : data(d), left(nullptr), right(nullptr), parent(p), color(Color::RED) {}
  };

  Node* root_;
  std::size_t size_;
  Compare comp_;

  // Recursively duplicate a subtree (used by copy constructor)
  Node* copy_tree(Node* src, Node* parent) {
    if (!src)
      return nullptr;
    Node* n = new Node(src->data, parent);
    n->color = src->color;
    n->left = copy_tree(src->left, n);
    n->right = copy_tree(src->right, n);
    return n;
  }

  // Post-order deletion: delete children before parent
  void destroy(Node* node) {
    if (!node)
      return;
    destroy(node->left);
    destroy(node->right);
    delete node;
  }

  // Iterative BST search using comparator
  Node* find_node(Node* node, const T& value) const {
    while (node) {
      if (comp_(value, node->data))
        node = node->left;
      else if (comp_(node->data, value))
        node = node->right;
      else
        return node; // Found
    }
    return nullptr;
  }

  // Recursive in-order traversal: left subtree, current node, right subtree
  template <typename Func>
  void in_order(Node* node, Func& func) const {
    if (!node)
      return;
    in_order(node->left, func);
    func(node->data);
    in_order(node->right, func);
  }

  // Left rotation: x's right child (y) becomes x's parent.
  //     x              y
  //    / \            / \
  //   a   y   -->   x   c
  //      / \       / \
  //     b   c     a   b
  void rotate_left(Node* x) {
    Node* y = x->right;
    x->right = y->left;       // x adopts y's left subtree
    if (y->left)
      y->left->parent = x;
    y->parent = x->parent;    // y takes x's position in the tree
    if (!x->parent)
      root_ = y;
    else if (x == x->parent->left)
      x->parent->left = y;
    else
      x->parent->right = y;
    y->left = x;              // x becomes y's left child
    x->parent = y;
  }

  // Right rotation: mirror of left rotation
  //       x            y
  //      / \          / \
  //     y   c  -->   a   x
  //    / \              / \
  //   a   b            b   c
  void rotate_right(Node* x) {
    Node* y = x->left;
    x->left = y->right;
    if (y->right)
      y->right->parent = x;
    y->parent = x->parent;
    if (!x->parent)
      root_ = y;
    else if (x == x->parent->right)
      x->parent->right = y;
    else
      x->parent->left = y;
    y->right = x;
    x->parent = y;
  }

  // Null nodes are considered BLACK (property 3)
  static Color color_of(Node* n) {
    return n ? n->color : Color::BLACK;
  }

  // Fix red-black violations after inserting a RED node z.
  // Violations occur when z's parent is also RED (property 4).
  // Three cases per side (mirror-symmetric for left/right):
  //   Case 1: Uncle is RED → recolor parent, uncle, grandparent
  //   Case 2: Uncle is BLACK, z is inner child → rotate z to outer
  //   Case 3: Uncle is BLACK, z is outer child → rotate grandparent
  void fix_insert(Node* z) {
    while (z != root_ && color_of(z->parent) == Color::RED) {
      Node* gp = z->parent->parent; // Grandparent
      if (!gp)
        break;

      if (z->parent == gp->left) {
        // Parent is left child of grandparent
        Node* uncle = gp->right;
        if (color_of(uncle) == Color::RED) {
          // Case 1: recolor and move up
          z->parent->color = Color::BLACK;
          uncle->color = Color::BLACK;
          gp->color = Color::RED;
          z = gp; // Continue fixing upward
        } else {
          if (z == z->parent->right) {
            // Case 2: z is right child (inner) → left rotate to make it outer
            z = z->parent;
            rotate_left(z);
          }
          // Case 3: z is left child (outer) → right rotate grandparent
          z->parent->color = Color::BLACK;
          gp->color = Color::RED;
          rotate_right(gp);
        }
      } else {
        // Mirror: parent is right child of grandparent
        Node* uncle = gp->left;
        if (color_of(uncle) == Color::RED) {
          z->parent->color = Color::BLACK;
          uncle->color = Color::BLACK;
          gp->color = Color::RED;
          z = gp;
        } else {
          if (z == z->parent->left) {
            z = z->parent;
            rotate_right(z);
          }
          z->parent->color = Color::BLACK;
          gp->color = Color::RED;
          rotate_left(gp);
        }
      }
    }
    root_->color = Color::BLACK;
  }
};

} // namespace fdr

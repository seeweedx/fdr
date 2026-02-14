#pragma once

#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace fdr {

template <typename T, typename Compare = std::less<T>>
class RBTree {
public:
  RBTree(Compare comp = Compare{}) : root_(nullptr), size_(0), comp_(comp) {}

  ~RBTree() { destroy(root_); }

  // Copy
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

  // Move
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

  void insert(const T& value) {
    // Iterative BST insert
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

    Node* n = new Node(value, parent);
    ++size_;

    if (!parent) {
      root_ = n;
    } else if (comp_(value, parent->data)) {
      parent->left = n;
    } else {
      parent->right = n;
    }

    fix_insert(n);
    root_->color = Color::BLACK;
  }

  bool contains(const T& value) const {
    return find_node(root_, value) != nullptr;
  }

  // In-order traversal
  template <typename Func>
  void for_each(Func func) const {
    in_order(root_, func);
  }

  // Collect all elements in sorted order
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

    Node(const T& d, Node* p = nullptr)
      : data(d), left(nullptr), right(nullptr), parent(p), color(Color::RED) {}
  };

  Node* root_;
  std::size_t size_;
  Compare comp_;

  Node* copy_tree(Node* src, Node* parent) {
    if (!src)
      return nullptr;
    Node* n = new Node(src->data, parent);
    n->color = src->color;
    n->left = copy_tree(src->left, n);
    n->right = copy_tree(src->right, n);
    return n;
  }

  void destroy(Node* node) {
    if (!node)
      return;
    destroy(node->left);
    destroy(node->right);
    delete node;
  }

  Node* find_node(Node* node, const T& value) const {
    while (node) {
      if (comp_(value, node->data))
        node = node->left;
      else if (comp_(node->data, value))
        node = node->right;
      else
        return node;
    }
    return nullptr;
  }

  template <typename Func>
  void in_order(Node* node, Func& func) const {
    if (!node)
      return;
    in_order(node->left, func);
    func(node->data);
    in_order(node->right, func);
  }

  void rotate_left(Node* x) {
    Node* y = x->right;
    x->right = y->left;
    if (y->left)
      y->left->parent = x;
    y->parent = x->parent;
    if (!x->parent)
      root_ = y;
    else if (x == x->parent->left)
      x->parent->left = y;
    else
      x->parent->right = y;
    y->left = x;
    x->parent = y;
  }

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

  static Color color_of(Node* n) {
    return n ? n->color : Color::BLACK;
  }

  void fix_insert(Node* z) {
    while (z != root_ && color_of(z->parent) == Color::RED) {
      Node* gp = z->parent->parent;
      if (!gp)
        break;

      if (z->parent == gp->left) {
        Node* uncle = gp->right;
        if (color_of(uncle) == Color::RED) {
          z->parent->color = Color::BLACK;
          uncle->color = Color::BLACK;
          gp->color = Color::RED;
          z = gp;
        } else {
          if (z == z->parent->right) {
            z = z->parent;
            rotate_left(z);
          }
          z->parent->color = Color::BLACK;
          gp->color = Color::RED;
          rotate_right(gp);
        }
      } else {
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

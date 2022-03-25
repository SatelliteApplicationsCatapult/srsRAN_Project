
#include "srsgnb/adt/byte_buffer.h"
#include "srsgnb/support/test_utils.h"
#include <list>

using namespace srsgnb;

static_assert(std::is_same<byte_buffer::value_type, uint8_t>::value, "Invalid valid_type");
static_assert(std::is_same<byte_buffer::iterator::value_type, uint8_t>::value, "Invalid valid_type");
static_assert(std::is_same<byte_buffer::const_iterator::value_type, uint8_t>::value, "Invalid valid_type");
static_assert(std::is_same<byte_buffer::iterator::reference, uint8_t&>::value, "Invalid reference type");
static_assert(std::is_same<byte_buffer::const_iterator::reference, const uint8_t&>::value, "Invalid reference type");
static_assert(std::is_same<byte_buffer::const_iterator::pointer, const uint8_t*>::value, "Invalid pointer type");

std::vector<uint8_t> make_small_vec()
{
  return {1, 2, 3, 4, 5, 6};
}

std::vector<uint8_t> make_big_vec()
{
  std::vector<uint8_t> vec(byte_buffer_segment::SEGMENT_SIZE);
  for (size_t i = 0; i < vec.size(); ++i) {
    vec[i] = i;
  }
  return vec;
}

void test_byte_buffer_append()
{
  byte_buffer pdu;
  TESTASSERT(pdu.empty());
  TESTASSERT_EQ(0, pdu.length());

  std::vector<uint8_t> bytes = make_small_vec();
  pdu.append(bytes);
  TESTASSERT_EQ(pdu.length(), bytes.size());

  // create a new segment during the append.
  bytes = make_big_vec();
  pdu.append(bytes);
  TESTASSERT_EQ(pdu.length(), bytes.size() + 6);
}

void test_byte_buffer_prepend()
{
  byte_buffer          pdu;
  std::vector<uint8_t> bytes = {1, 2, 3, 4, 5, 6};

  pdu.prepend(bytes);
  TESTASSERT_EQ(pdu.length(), bytes.size());

  bytes.resize(byte_buffer_segment::SEGMENT_SIZE);
  for (size_t i = 0; i < bytes.size(); ++i) {
    bytes[i] = i;
  }
  pdu.prepend(bytes);
  TESTASSERT_EQ(pdu.length(), bytes.size() + 6);
}

void test_byte_buffer_compare()
{
  byte_buffer          pdu, pdu2, pdu3, pdu4;
  std::vector<uint8_t> bytes  = {1, 2, 3, 4, 5, 6};
  std::vector<uint8_t> bytes2 = {1, 2, 3, 4, 5};
  std::vector<uint8_t> bytes3 = {2, 2, 3, 4, 5, 6};

  pdu.append(bytes);

  TESTASSERT(pdu == bytes);
  TESTASSERT(bytes == pdu);
  TESTASSERT(bytes2 != pdu);
  TESTASSERT(bytes3 != pdu);

  pdu2.append(bytes);
  pdu3.append(bytes2);
  pdu4.append(bytes3);

  TESTASSERT(pdu == pdu2);
  TESTASSERT(pdu != pdu3);
  TESTASSERT(pdu != pdu4);
  TESTASSERT(pdu2 != pdu4);
  TESTASSERT(pdu3 != pdu4);

  // create a new segment during the append.
  bytes2.resize(byte_buffer_segment::SEGMENT_SIZE);
  for (size_t i = 0; i < bytes2.size(); ++i) {
    bytes2[i] = i;
  }
  pdu.append(bytes2);
  bytes.insert(bytes.end(), bytes2.begin(), bytes2.end());
  TESTASSERT(pdu == bytes);

  // create a new segment during the prepend.
  pdu.prepend(bytes2);
  bytes.insert(bytes.begin(), bytes2.begin(), bytes2.end());
  TESTASSERT(pdu == bytes);
}

void test_byte_buffer_iterator()
{
  byte_buffer pdu;

  std::vector<uint8_t> bytes = make_small_vec();
  pdu.append(bytes);

  // iterator
  size_t i = 0;
  for (byte_buffer::iterator it = pdu.begin(); it != pdu.end(); ++it, ++i) {
    TESTASSERT_EQ(bytes[i], *it);
  }
  TESTASSERT_EQ(bytes.size(), i);

  // const iterator
  i = 0;
  for (byte_buffer::const_iterator it = pdu.cbegin(); it != pdu.cend(); ++it, ++i) {
    TESTASSERT_EQ(bytes[i], *it);
  }
  TESTASSERT_EQ(bytes.size(), i);

  // distance
  TESTASSERT_EQ(bytes.size(), pdu.length());
  TESTASSERT_EQ(bytes.size(), (size_t)(pdu.end() - pdu.begin()));
  TESTASSERT_EQ(bytes.size() - 2, (size_t)(pdu.end() - (pdu.begin() + 2)));

  // multiple segments
  std::vector<uint8_t> bytes2 = make_big_vec();
  pdu.append(bytes2);
  std::vector<uint8_t> bytes_concat = bytes;
  bytes_concat.insert(bytes_concat.end(), bytes2.begin(), bytes2.end());

  // iterator
  i = 0;
  for (byte_buffer::iterator it = pdu.begin(); it != pdu.end(); ++it, ++i) {
    TESTASSERT_EQ(bytes_concat[i], *it);
  }
  TESTASSERT_EQ(bytes_concat.size(), i);

  // const iterator
  i = 0;
  for (byte_buffer::const_iterator it = pdu.cbegin(); it != pdu.cend(); ++it, ++i) {
    TESTASSERT_EQ(bytes_concat[i], *it);
  }
  TESTASSERT_EQ(bytes_concat.size(), i);

  // distance
  TESTASSERT_EQ(bytes_concat.size(), pdu.length());
  TESTASSERT_EQ(bytes_concat.size(), (size_t)(pdu.end() - pdu.begin()));
  TESTASSERT_EQ(bytes_concat.size() - 2, (size_t)(pdu.end() - (pdu.begin() + 2)));
}

void test_byte_buffer_clone()
{
  byte_buffer pdu;

  std::vector<uint8_t> bytes = {1, 2, 3, 4, 5, 6};
  pdu.append(bytes);

  byte_buffer pdu2;
  pdu2 = pdu.clone();
  TESTASSERT(not pdu2.empty() and not pdu.empty());
  TESTASSERT_EQ(pdu.length(), pdu2.length());
  TESTASSERT(pdu == pdu2);
  TESTASSERT(pdu2 == bytes);

  pdu2.append(bytes);
  TESTASSERT(pdu != pdu2);
  TESTASSERT(pdu2 != bytes);
  TESTASSERT_EQ(pdu.length() * 2, pdu2.length());
}

void test_byte_buffer_move()
{
  byte_buffer          pdu;
  std::vector<uint8_t> bytes = {1, 2, 3, 4, 5, 6};
  pdu.append(bytes);

  byte_buffer pdu2{std::move(pdu)};
  TESTASSERT(not pdu2.empty() and pdu.empty());
  TESTASSERT(pdu == bytes);
}

void test_byte_buffer_formatter()
{
  byte_buffer          pdu;
  std::vector<uint8_t> bytes = {1, 2, 3, 4, 15, 16, 255};
  pdu.append(bytes);

  fmt::print("PDU: {}\n", pdu);
  std::string result = fmt::format("{}", pdu);
  TESTASSERT(result == "01 02 03 04 0f 10 ff");
}

void test_byte_buffer_view()
{
  byte_buffer          pdu;
  std::vector<uint8_t> bytes = make_small_vec();
  pdu.append(bytes);

  byte_buffer_view view = pdu;

  TESTASSERT(not view.empty());
  TESTASSERT_EQ(6, view.length());
  TESTASSERT_EQ(6, view.end() - view.begin());
  TESTASSERT_EQ(4, view.slice(0, 4).length());
  TESTASSERT_EQ(4, view.slice(2, 4).length());
}

int main()
{
  test_byte_buffer_append();
  test_byte_buffer_prepend();
  test_byte_buffer_iterator();
  test_byte_buffer_compare();
  test_byte_buffer_clone();
  test_byte_buffer_move();
  test_byte_buffer_formatter();
  test_byte_buffer_view();
}